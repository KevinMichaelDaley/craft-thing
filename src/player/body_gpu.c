#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/body_gpu.h"
#include "../vulkan/reactive_gpu_internal.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "../vulkan/runtime_body_gpu.h"

static bool beaver_body_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}

static bool beaver_body_created_by(const dc_body_storage_owner_t *owner,uint64_t transaction) {
    return owner->state==DC_BODY_OWNER_LIVE&&owner->source_transaction.low==(uint32_t)transaction&&
        owner->source_transaction.high==(uint32_t)(transaction>>32);
}

/* Rewind only when this shape ends at the pool cursor, so earlier geometry
 * stays valid. */
static uint32_t beaver_body_geometry_tail(dc_gpu_t *g,uint32_t slot) {
    const dc_gpu_body_record_t *body=dc_gpu_body_record(g,slot);
    if(!(body->piece_count&DC_GPU_PIXEL_BODY_FLAG)) return UINT32_MAX;
    uint32_t pool_at=dc_gpu_pixel_pool_word_offset(g);
    const uint32_t *words=g->body_mapped,*pool=words+pool_at;
    uint64_t first=(uint64_t)pool_at+DC_GPU_PIXEL_POOL_HEADER_WORDS+
        (uint64_t)pool[0]*dc_gpu_pixel_pool_capacity(g);
    if(body->compound_reserved<first||body->compound_reserved>=first+pool[1]) return UINT32_MAX;
    uint32_t offset=(uint32_t)(body->compound_reserved-first);
    return words[body->compound_reserved]==pool[1]-offset?offset:UINT32_MAX;
}

bool fr_beaver_body_admit_prepared(dc_gpu_t *g,uint64_t identity,uint64_t transaction,
    uint32_t slot,dc_chunk_coord_t spawn,int32_t x,int32_t y,
    fr_beaver_body_prepare_fn prepare,void *user,fr_beaver_body_t *out,bool *world_consistent,
    char *err,uint32_t cap) {
    if(world_consistent) *world_consistent=true;
    const int32_t chunk_extent=DC_CHUNK_SIDE*DC_FLUID_FULL;
    if(!g||!identity||!transaction||slot>=dc_gpu_slot_capacity(g)||!out||
       x<0||y<0||x>=chunk_extent||y>=chunk_extent)
        return beaver_body_error(err,cap,"Invalid cold player admission");
    for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++)
        if(beaver_body_created_by(dc_gpu_body_owner(g,s),transaction))
            return beaver_body_error(err,cap,"Player admission transaction already exists");
    dc_chunk_t *chunk=malloc(2u*sizeof(*chunk));
    uint32_t *objects=calloc(dc_gpu_slot_capacity(g)*DC_CHUNK_CELLS,sizeof(*objects));
    fr_reactive_record_t *records=calloc(FR_BEAVER_PIXELS,sizeof(*records));
    if(!chunk||!objects||!records) {
        free(chunk);free(objects);free(records);
        return beaver_body_error(err,cap,"Cannot stage canonical player import");
    }
    dc_chunk_t *stage=chunk+1;
    const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
    bool okay=registry[5]!=0;
    if(okay) memcpy(&chunk->coord,registry,sizeof(chunk->coord));
    else beaver_body_error(err,cap,"Player staging slot is not resident");
    if(okay) okay=dc_gpu_download_chunk(g,slot,chunk,err,cap);
    for(uint32_t c=0;c<DC_CHUNK_CELLS&&okay;c++)
        if(chunk->cells[c].material||chunk->cells[c].fluid_mass) okay=beaver_body_error(err,cap,"Player staging chunk must be empty");
    if(okay) *stage=*chunk;
    uint32_t count=0,min_x=FR_BEAVER_WIDTH,min_y=FR_BEAVER_HEIGHT,max_x=0,max_y=0;
    uint64_t high=g->reactive?g->reactive->high_id:0;
    for(uint32_t source=1;source<=FR_BEAVER_PIXELS&&okay;source++) {
        fr_beaver_source_t meta;if(!fr_beaver_source(source,&meta)) continue;
        uint32_t rx=(source-1u)%FR_BEAVER_WIDTH,ry=(source-1u)/FR_BEAVER_WIDTH;
        uint32_t cell=ry*DC_CHUNK_SIDE+rx;
        const fr_reactive_profile_t *profile=fr_reactive_profile(meta.reactive_material);
        if(!profile||high>=UINT64_MAX-1u) {okay=beaver_body_error(err,cap,"Invalid player material or exhausted parcel IDs");break;}
        uint32_t mass=(uint32_t)lroundf(profile->density*DC_FLUID_FULL);
        stage->cells[cell].material=(uint16_t)fr_prop_terrain_material(meta.prop_material);
        objects[slot*DC_CHUNK_CELLS+cell]=1;
        fr_reactive_record_t *record=&records[count++];
        record->owner=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={0,0,rx,ry,0}};
        okay=fr_reactive_state_default(meta.reactive_material,mass,0,293.15f,++high,&record->state,err,cap);
        if(rx<min_x) min_x=rx;
        if(ry<min_y) min_y=ry;
        if(rx>max_x) max_x=rx;
        if(ry>max_y) max_y=ry;
    }
    bool staged=false;
    if(okay) okay=fr_gpu_reactive_reserve_import(g,count,err,cap);
    if(okay) {
        staged=true;
        okay=dc_gpu_upload_chunk(g,slot,stage,err,cap)&&
            fr_gpu_admit_rigid_objects(g,transaction,objects,dc_gpu_slot_capacity(g)*DC_CHUNK_CELLS,err,cap);
    }
    fr_beaver_body_t result={.binding={.identity=identity},.rest_origin_x=min_x,.rest_origin_y=min_y,
        .width=max_x-min_x+1,.height=max_y-min_y+1};
    uint32_t bodies=0,body_slot=UINT32_MAX;
    if(okay) for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,s);
        if(!beaver_body_created_by(owner,transaction)) continue;
        result.binding.body_id=owner->runtime_id;result.binding.generation=owner->generation;bodies++;
        body_slot=s;
    }
    if(okay&&bodies!=1) okay=beaver_body_error(err,cap,"Canonical player did not admit as one connected body");
    if(okay) okay=fr_gpu_pose_authored_rigid(g,result.binding.body_id,spawn,x,y,0,err,cap)&&
        dc_gpu_set_body_motion(g,result.binding.body_id,0,0,DC_GPU_BODY_LOCK_ROTATION,err,cap);
    if(okay&&prepare) okay=prepare(user,&result,err,cap);
    for(uint32_t i=0;i<count&&okay;i++) {
        fr_reactive_owner_t *owner=&records[i].owner;
        owner->key[0]=result.binding.body_id;
        owner->key[1]=result.binding.generation;
        owner->key[2]-=min_x;
        owner->key[3]-=min_y;
    }
    if(okay) okay=fr_gpu_reactive_attach_authored_body(g,body_slot,records,count,err,cap);
    if(!okay&&staged) {
        bool restored=true;
        uint32_t geometry_tail=UINT32_MAX;
        for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
            const dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,s);
            if(!beaver_body_created_by(owner,transaction)) continue;
            uint32_t tail=beaver_body_geometry_tail(g,s);
            if(!dc_gpu_remove_body(g,owner->runtime_id,NULL,0)) restored=false;
            else if(tail!=UINT32_MAX) geometry_tail=tail;
        }
        if(!dc_gpu_upload_chunk(g,slot,chunk,NULL,0)) restored=false;
        if(restored&&geometry_tail!=UINT32_MAX) {
            uint32_t *pool=(uint32_t *)g->body_mapped+dc_gpu_pixel_pool_word_offset(g);
            pool[1]=geometry_tail;
        }
        if(!restored||!dc_gpu_set_body_origin(g,g->body_origin,NULL,0)) {
            if(world_consistent) *world_consistent=false;
            beaver_body_error(err,cap,"Cannot restore the world after failed player admission");
        }
    }
    if(okay) *out=result;
    free(chunk);free(objects);free(records);return okay;
}

bool fr_beaver_body_admit(dc_gpu_t *g,uint64_t identity,uint64_t transaction,
    uint32_t slot,dc_chunk_coord_t spawn,int32_t x,int32_t y,fr_beaver_body_t *out,
    char *err,uint32_t cap) {
    return fr_beaver_body_admit_prepared(g,identity,transaction,slot,spawn,x,y,
        NULL,NULL,out,NULL,err,cap);
}
