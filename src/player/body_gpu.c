#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/body_gpu.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "../vulkan/runtime_body_gpu.h"

static bool beaver_body_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}

bool fr_beaver_body_admit(dc_gpu_t *g,uint64_t identity,uint64_t transaction,
    uint32_t slot,dc_chunk_coord_t spawn,int32_t x,int32_t y,fr_beaver_body_t *out,
    char *err,uint32_t cap) {
    if(!g||!identity||!transaction||slot>=DC_GPU_CHUNK_SLOTS||!out||
       x<0||y<0||x>=64*65536||y>=64*65536)
        return beaver_body_error(err,cap,"Invalid cold player admission");
    for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,s);
        if(owner->state==DC_BODY_OWNER_LIVE&&owner->source_transaction.low==(uint32_t)transaction&&
           owner->source_transaction.high==(uint32_t)(transaction>>32))
            return beaver_body_error(err,cap,"Player admission transaction already exists");
    }
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    uint32_t *objects=calloc(DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS,sizeof(*objects));
    fr_reactive_attachment_t *attachments=calloc(FR_BEAVER_PIXELS,sizeof(*attachments));
    if(!chunk||!objects||!attachments) {
        free(chunk);free(objects);free(attachments);
        return beaver_body_error(err,cap,"Cannot stage canonical player import");
    }
    bool okay=dc_gpu_download_chunk(g,slot,chunk,err,cap);
    for(uint32_t c=0;c<DC_CHUNK_CELLS&&okay;c++)
        if(chunk->cells[c].material||chunk->cells[c].fluid_mass) okay=beaver_body_error(err,cap,"Player staging chunk must be empty");
    uint32_t count=0,min_x=40,min_y=40,max_x=0,max_y=0;
    uint64_t high=g->reactive?g->reactive->high_id:0;
    for(uint32_t source=1;source<=FR_BEAVER_PIXELS&&okay;source++) {
        fr_beaver_source_t meta;if(!fr_beaver_source(source,&meta)) continue;
        uint32_t rx=(source-1)%40,ry=(source-1)/40,cell=ry*64+rx;
        uint32_t material=fr_prop_terrain_material(meta.prop_material);
        const fr_reactive_profile_t *profile=fr_reactive_profile(material);
        if(!profile||high>=UINT64_MAX-1u) {okay=beaver_body_error(err,cap,"Invalid player material or exhausted parcel IDs");break;}
        uint32_t mass=(uint32_t)lroundf(profile->density*DC_FLUID_FULL);
        chunk->cells[cell].material=(uint16_t)material;objects[slot*4096u+cell]=1;
        fr_reactive_attachment_t *a=&attachments[count++];a->local_index=cell;
        a->record.owner=fr_reactive_cell_owner(chunk->coord,cell);
        okay=fr_reactive_state_default(material,mass,0,293.15f,++high,&a->record.state,err,cap);
        if(rx<min_x) min_x=rx;
        if(ry<min_y) min_y=ry;
        if(rx>max_x) max_x=rx;
        if(ry>max_y) max_y=ry;
    }
    fr_reactive_chunk_t channel={.count=count,.next_id=high+1u,.attachments=attachments};
    if(okay) okay=fr_reactive_chunk_valid(&channel,chunk,err,cap)&&
        fr_gpu_reactive_reserve_import(g,count,err,cap)&&
        dc_gpu_upload_chunk(g,slot,chunk,err,cap)&&
        fr_gpu_reactive_import_chunk(g,slot,chunk,&channel,err,cap)&&
        fr_gpu_admit_rigid_objects(g,transaction,objects,DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS,err,cap);
    fr_beaver_body_t result={.binding={.identity=identity},.rest_origin_x=min_x,.rest_origin_y=min_y,
        .width=max_x-min_x+1,.height=max_y-min_y+1};
    uint32_t bodies=0;
    if(okay) for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,s);
        if(owner->state!=DC_BODY_OWNER_LIVE||owner->source_transaction.low!=(uint32_t)transaction||
           owner->source_transaction.high!=(uint32_t)(transaction>>32)) continue;
        result.binding.body_id=owner->runtime_id;result.binding.generation=owner->generation;bodies++;
    }
    if(okay&&bodies!=1) okay=beaver_body_error(err,cap,"Canonical player did not admit as one connected body");
    if(okay) okay=fr_gpu_pose_authored_rigid(g,result.binding.body_id,spawn,x,y,0,err,cap)&&
        dc_gpu_set_body_motion(g,result.binding.body_id,0,0,DC_GPU_BODY_LOCK_ROTATION,err,cap);
    if(okay) *out=result;
    free(chunk);free(objects);free(attachments);return okay;
}
