#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "beaver_fire_demo.h"
#include "level_internal.h"
#include "player_session.h"
#include "smoke_profile.h"
#include "view_config.h"

#define DEMO_TRANSACTION UINT64_C(0x424f4e4600000001)
static bool demo_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}
static const dc_beaver_fire_log_t *demo_log_at(const dc_beaver_fire_plan_t *p,int64_t x,int64_t y) {
    for(uint32_t n=0;n<p->count;n++) {
        const dc_beaver_fire_log_t *log=&p->logs[n];
        if(x>=log->x&&y>=log->y&&x<log->x+log->width&&y<log->y+log->height) return log;
    }
    return NULL;
}
static bool demo_wood(dc_level_view_t *v,const dc_beaver_fire_plan_t *p,char *err,uint32_t cap) {
    uint32_t *objects=calloc(DC_GPU_CHUNK_SLOTS*4096u,sizeof(*objects));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    if(!objects||!chunk) {free(objects);free(chunk);return demo_error(err,cap,"Cannot stage demo wood");}
    uint32_t total=0;for(uint32_t n=0;n<p->count;n++) total+=p->logs[n].width*p->logs[n].height;
    bool okay=fr_gpu_reactive_reserve_import(v->gpu,total,err,cap);
    for(uint32_t slot=0;slot<v->table.capacity&&okay;slot++) {
        dc_chunk_slot_t *owner=&v->table.slots[slot];
        if(owner->state!=DC_SLOT_ACTIVE&&owner->state!=DC_SLOT_SLEEPING) continue;
        uint32_t added=0;
        for(uint32_t i=0;i<4096;i++) if(demo_log_at(p,owner->coord.x*64+i%64,owner->coord.y*64+i/64)) added++;
        if(!added) continue;
        chunk->coord=owner->coord;
        okay=dc_gpu_download_chunk(v->gpu,slot,chunk,err,cap);
        fr_reactive_chunk_t packet={0};
        if(okay) okay=fr_gpu_reactive_export_chunk(v->gpu,slot,chunk,&packet,err,cap);
        if(!okay) {fr_reactive_chunk_release(&packet);break;}
        fr_reactive_attachment_t *attachments=realloc(packet.attachments,(size_t)(packet.count+added)*sizeof(*attachments));
        if(!attachments) {fr_reactive_chunk_release(&packet);okay=demo_error(err,cap,"Cannot stage wood parcel packet");break;}
        packet.attachments=attachments;
        uint64_t high=v->gpu->reactive->high_id;
        for(uint32_t i=0;i<4096&&okay;i++) {
            const dc_beaver_fire_log_t *log=demo_log_at(p,owner->coord.x*64+i%64,owner->coord.y*64+i/64);
            if(!log) continue;
            if(chunk->cells[i].material||chunk->cells[i].fluid_mass||high>=UINT64_MAX-1) {okay=demo_error(err,cap,"Unlit pile placement overlaps existing matter");break;}
            chunk->cells[i]=(dc_cell_t){.material=DC_MATERIAL_WOOD};objects[slot*4096u+i]=log->object;
            fr_reactive_attachment_t *a=&packet.attachments[packet.count++];a->local_index=i;
            a->record.owner=fr_reactive_cell_owner(chunk->coord,i);
            const fr_reactive_profile_t *profile=fr_reactive_profile(DC_MATERIAL_WOOD);
            okay=fr_reactive_state_default(DC_MATERIAL_WOOD,(uint32_t)lroundf(profile->density*65536.f),0,
                p->temperature_K,++high,&a->record.state,err,cap);
        }
        packet.next_id=high+1;
        if(okay) okay=fr_reactive_chunk_valid(&packet,chunk,err,cap)&&dc_gpu_upload_chunk(v->gpu,slot,chunk,err,cap)&&
            fr_gpu_reactive_import_chunk(v->gpu,slot,chunk,&packet,err,cap)&&dc_chunk_table_mark_dirty(&v->table,slot);
        fr_reactive_chunk_release(&packet);
    }
    if(okay) okay=fr_gpu_admit_rigid_objects(v->gpu,DEMO_TRANSACTION,objects,DC_GPU_CHUNK_SLOTS*4096u,err,cap);
    free(objects);free(chunk);return okay;
}

static bool configure_gas(dc_level_view_t *view,char *err,uint32_t cap) {
    dc_beaver_fire_plan_t plan;
    if(!view||!dc_beaver_fire_plan(VIEW_WIDTH,VIEW_HEIGHT,&plan)||!dc_smoke_profile_defaults()) return demo_error(err,cap,"Invalid fire demo view or smoke profile");
    fr_gas_model_t gas={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    return fr_gas_gpu_enable(view->gpu,&gas,.0012f,293.15f,err,cap)&&
        fr_gas_gpu_set_pressure_policy(view->gpu,FR_GAS_PRESSURE_PRACTICAL,err,cap)&&
        fr_gas_gpu_set_gravity(view->gpu,900,err,cap);
}

bool dc_beaver_fire_configure(dc_level_view_t *view,char *err,uint32_t cap) {
    return configure_gas(view,err,cap);
}

bool dc_beaver_fire_setup(dc_level_view_t *view,char *err,uint32_t cap) {
    dc_beaver_fire_plan_t plan;
    if(!view||!dc_beaver_fire_plan(VIEW_WIDTH,VIEW_HEIGHT,&plan)||!dc_smoke_profile_defaults()) return demo_error(err,cap,"Invalid fire demo view or smoke profile");
    if(!dc_players_enable_at(view,plan.player_x,plan.player_y,err,cap)) return false;
    if(!view->restored&&!demo_wood(view,&plan,err,cap)) return false;
    if(!configure_gas(view,err,cap)) return false;
    for(uint32_t slot=0;slot<view->table.capacity;slot++) {
        if(view->gas_admitted[slot]) continue;
        dc_chunk_slot_t *owner=&view->table.slots[slot];
        if(owner->state!=DC_SLOT_ACTIVE&&owner->state!=DC_SLOT_SLEEPING) continue;
        if(view->restored) return demo_error(err,cap,"Saved fire demo has missing gas stock");
        if(!fr_gas_gpu_queue_ambient_chunk(view->gpu,slot,err,cap)) return false;
        view->gas_admitted[slot]=true;
    }
    if(!fr_gas_gpu_initialize_queued(view->gpu,err,cap)||!fr_combustion_gpu_enable(view->gpu,err,cap)||
       !dc_players_fire_ray(view,(fr_fire_ray_config_t){VIEW_WIDTH,4,.4f,1800},err,cap)) return false;
    view->smoke_box=view->smoke_ready=true;
    printf("Beaver fire ray demo: %u unlit ambient wood logs, %s | A/D move, Space jump, Q facing ray, Esc save/exit\n",
        plan.count,view->restored?"restored":"fresh");
    return dc_level_view_set_title(view,"Beaver Fire Ray | A/D move | Space jump | Q cast | Esc save and exit");
}
