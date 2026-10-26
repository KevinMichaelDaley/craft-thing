#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/gas_gpu.h"
#include "level_internal.h"
#include "smoke_box.h"
#include "view_config.h"
static bool error(char *err,uint32_t cap,const char *message) {if(err&&cap) snprintf(err,cap,"%s",message);return false;}
dc_level_view_t *dc_level_view_create_smoke_box(const char *directory,uint64_t seed,char *err,uint32_t cap) {
    const uint32_t surface=96;
    dc_ground_profile_t ground={.width=1,.soil_depth=1,.surface=&surface};
    dc_level_view_t *view=dc_level_view_create_ground(directory,seed,&ground,err,cap);
    if(view&&dc_level_view_setup_smoke_box(view,err,cap)) return view;
    if(view) {
        dc_stream_destroy(view->stream);dc_gpu_destroy(view->gpu);
        dc_chunk_table_destroy(&view->table);free(view);
    }
    return NULL;
}
static bool valve(dc_level_view_t *view,uint32_t index,bool closed,char *err,uint32_t cap) {
    static const int32_t rectangle[3][5]={{124,47,8,1,1},{324,47,8,1,1},{399,-48,1,16,0}};
    if(!view||index>=3) return error(err,cap,"Invalid smoke fixture valve");
    int64_t x=(int64_t)rectangle[index][0]-view->mapped_origin.x*64+DC_CHUNK_SIDE;
    int64_t y=(int64_t)rectangle[index][1]-view->mapped_origin.y*64+DC_CHUNK_SIDE;
    if(x<0||y<0||x+rectangle[index][2]>SIM_WIDTH||y+rectangle[index][3]>SIM_HEIGHT)
        return error(err,cap,"Smoke fixture valve is outside the resident window");
    if(!fr_gas_gpu_gate(view->gpu,(uint32_t)x,(uint32_t)y,rectangle[index][2],rectangle[index][3],rectangle[index][4],closed,err,cap)) return false;
    if(closed) view->smoke_closed|=1u<<index;else view->smoke_closed&=~(1u<<index);return true;
}
static bool configure_gas(dc_level_view_t *view,char *err,uint32_t cap) {
    fr_gas_model_t model=dc_smoke_box_model();const char *large=getenv("DC_GAS_LARGE_CFL"),*practical=getenv("DC_GAS_PRACTICAL");
    bool large_cfl=large&&strcmp(large,"0")!=0,bounded=practical&&strcmp(practical,"0")!=0;
    return fr_gas_gpu_enable(view->gpu,&model,.0012f,293.15f,err,cap)&&
        fr_gas_gpu_set_pressure_policy(view->gpu,bounded?FR_GAS_PRESSURE_PRACTICAL:(large_cfl?FR_GAS_PRESSURE_LARGE_CFL:FR_GAS_PRESSURE_IMPLICIT),err,cap)&&
        fr_gas_gpu_set_gravity(view->gpu,900,err,cap);
}
static bool restore_valve_state(dc_level_view_t *view,char *err,uint32_t cap) {
    static const int32_t point[3][3]={{124,47,1},{324,47,1},{399,-48,0}};
    fr_gas_checkpoint_chunk_t *gas=malloc(sizeof(*gas));if(!gas) return error(err,cap,"Cannot stage cold valve metadata");
    bool okay=true;view->smoke_closed=0;
    for(uint32_t i=0;okay&&i<3u;i++) {
        dc_chunk_coord_t coord={point[i][0]/64,point[i][1]>=0?point[i][1]/64:(point[i][1]-63)/64};uint32_t slot;
        okay=dc_chunk_table_find(&view->table,coord,&slot)&&fr_gas_gpu_export_checkpoint(view->gpu,slot,gas,err,cap);
        uint32_t local=(uint32_t)(point[i][1]-coord.y*64)*64u+(uint32_t)(point[i][0]-coord.x*64);
        if(okay&&(gas->gates[local]&(1u<<point[i][2]))!=0u) view->smoke_closed|=1u<<i;
    }
    free(gas);return okay;
}
bool dc_level_view_setup_smoke_box(dc_level_view_t *view,char *err,uint32_t cap) {
    if(!view||VIEW_WIDTH!=480||VIEW_HEIGHT!=270) return error(err,cap,"Smoke box requires quarter-native placement resolution");
    if(view->smoke_box) return true;
    if(view->restored) {
        if(!configure_gas(view,err,cap)) return false;
        view->smoke_box=true;
        if(!dc_level_view_set_spring_enabled(view,false)||!dc_level_view_wait_visible(view,120000,err,cap)) return false;
        for(uint32_t i=0;i<view->table.capacity;i++) if(!view->gas_admitted[i])
            return error(err,cap,"Saved smoke scene is missing an authoritative gas chunk");
        bool okay=dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO)&&restore_valve_state(view,err,cap);
        view->smoke_ready=okay;return okay;
    }
    if(view->origin.x!=0||view->origin.y!=INITIAL_CHUNK_Y) return error(err,cap,"Fresh smoke box requires the initial world origin");
    if(!dc_level_view_set_spring_enabled(view,false)||!dc_level_view_wait_visible(view,120000,err,cap)) return false;
    uint32_t count=view->table.capacity;dc_chunk_coord_t *coords=calloc(count,sizeof(*coords));
    dc_smoke_box_chunk_t *chunks=calloc(count,sizeof(*chunks));
    if(!coords||!chunks) {free(coords);free(chunks);return error(err,cap,"Cannot preflight smoke box");}
    for(uint32_t i=0;i<count;i++) coords[i]=view->table.slots[i].coord;
    bool okay=dc_smoke_box_build(coords,count,chunks,err,cap);free(coords);
    for(uint32_t i=0;okay&&i<count;i++) okay=dc_gpu_upload_chunk(view->gpu,i,chunks[i].chunk,err,cap)&&
        fr_renderer_upload_chunk_colors(view->gpu,i,chunks[i].colors,err,cap)&&dc_chunk_table_mark_dirty(&view->table,i);
    if(okay) okay=dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO)&&dc_gpu_render_chunks(view->gpu,err,cap)&&
        configure_gas(view,err,cap);
    for(uint32_t i=0;okay&&i<count;i++) {
        okay=fr_gas_gpu_seed_chunk(view->gpu,i,chunks[i].gas,chunks[i].void_fractions,4096,err,cap);
        view->gas_admitted[i]=okay;
    }
    dc_smoke_box_release(chunks,count);free(chunks);
    for(uint32_t i=0;okay&&i<3;i++) okay=valve(view,i,true,err,cap);
    if(okay) view->smoke_box=view->smoke_ready=true;
    return okay;
}
bool dc_level_view_smoke_valve(dc_level_view_t *view,uint32_t index,char *err,uint32_t cap) {
    if(!view||!view->smoke_box||index>=3) return error(err,cap,"Smoke box is not active");
    return valve(view,index,(view->smoke_closed&(1u<<index))==0u,err,cap);
}
bool dc_level_view_smoke_overlay(dc_level_view_t *view,uint32_t mode) {
    return view&&view->smoke_box&&fr_gas_gpu_overlay(view->gpu,mode);
}
bool dc_level_view_smoke_audit(dc_level_view_t *view,fr_gas_gpu_audit_t *out,char *err,uint32_t cap) {
    return view&&view->smoke_box&&fr_gas_gpu_audit(view->gpu,out,err,cap);
}
