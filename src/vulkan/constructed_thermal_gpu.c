#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/material_density.h"
#include "dungeoncraft/thermal_parameters.h"
#include "reactive_storage_gpu.h"
#include "runtime_body_internal.h"

static bool candidate(const dc_gpu_body_record_t *b) {
    return b->reserved[0]==DC_GPU_STRUCTURAL_TAG&&
        (b->shape.material==DC_GPU_BODY_STONE||b->shape.material==DC_GPU_BODY_WOOD)&&
        b->piece_count==0u&&(b->motion_flags&DC_BODY_MOTION_THERMAL_GRID)==0u;
}
static bool error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}
bool fr_gpu_reactive_admit_constructed_members(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!fr_gpu_reactive_initialize(g,err,cap)||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return false;
    dc_gpu_refresh_body_registry(g);uint32_t capacity=dc_gpu_body_capacity(g),parts=0;uint64_t cells=0;
    for(uint32_t i=0;i<capacity;i++) {const dc_gpu_body_record_t *b=dc_gpu_body_record(g,i);
        if(candidate(b)) {parts++;cells+=(uint64_t)b->body.width*b->body.height;}}
    if(!parts) return true;
    uint32_t pool_word=dc_gpu_pixel_pool_word_offset(g),*pool=(uint32_t *)g->body_mapped+pool_word;
    uint64_t required=(uint64_t)pool[1]+(uint64_t)parts*42u;
    if(cells>UINT32_MAX||required>UINT32_MAX||g->reactive->high_id>UINT64_MAX-cells)
        return error(err,cap,"Constructed member thermal address range exhausted");
    if(!dc_gpu_runtime_geometry_reserve(g,(uint32_t)required,err,cap)) return false;
    fr_reactive_arena_request_t *requests=calloc((size_t)cells,sizeof(*requests));
    fr_reactive_handle_t *handles=calloc((size_t)cells,sizeof(*handles));
    if(!requests||!handles) {free(requests);free(handles);return error(err,cap,"Cannot stage constructed member heat");}
    uint32_t n=0;uint64_t identity=g->reactive->high_id;bool okay=true;
    for(uint32_t i=0;i<capacity&&okay;i++) {const dc_gpu_body_record_t *b=dc_gpu_body_record(g,i);if(!candidate(b)) continue;
        double length=b->shape.vertices[1].x_fp/65536.,depth=b->shape.vertices[2].y_fp/65536.;
        uint32_t material=DC_BODY_TO_TERRAIN(b->shape.material);
        double density=DC_BODY_DENSITY(b->shape.material);
        if(b->body.width>16u||b->body.height>16u||length<=0||depth<=0) {okay=false;break;}
        for(uint32_t y=0;y<b->body.height&&okay;y++) for(uint32_t x=0;x<b->body.width&&okay;x++) {
            double area=fmin(1.,length-x)*fmin(1.,depth-y);
            uint32_t mass=(uint32_t)llround(area*density*65536.);
            fr_reactive_arena_request_t *r=requests+n++;r->operation=FR_REACTIVE_ARENA_ALLOCATE;
            r->record.owner=(fr_reactive_owner_t){FR_REACTIVE_OWNER_BODY,{b->body.id,dc_gpu_body_owner(g,i)->generation,x,y,0}};
            okay=mass&&fr_reactive_state_default(material,mass,0,FR_THERMAL_AMBIENT_K,++identity,&r->record.state,err,cap);
        }
    }
    uint32_t demand=0;
    if(okay) okay=fr_reactive_arena_apply(g,&g->reactive->arena,requests,n,handles,&demand,err,cap);
    if(okay) {
        pool_word=dc_gpu_pixel_pool_word_offset(g);pool=(uint32_t *)g->body_mapped+pool_word;
        uint32_t geometry_capacity=dc_gpu_pixel_pool_capacity(g),bank=pool[0],used=pool[1];n=0;
        uint32_t *reactive=g->reactive->shapes.mapped,allocated=((uint32_t *)g->reactive->arena.root.mapped)[12];
        for(uint32_t i=0;i<capacity;i++) {dc_gpu_body_record_t *b=dc_gpu_body_record(g,i);if(!candidate(b)) continue;
            uint32_t absolute=pool_word+DC_GPU_PIXEL_POOL_HEADER_WORDS+bank*geometry_capacity+used;
            uint32_t *shape=(uint32_t *)g->body_mapped+absolute;memset(shape,0,42u*4u);
            shape[0]=42;shape[1]=shape[2]=shape[3]=1;shape[6]=b->body.width;shape[7]=b->body.height;shape[8]=9;
            memset(dc_gpu_shape_colors(g,absolute),0,42u*128u);
            for(uint32_t y=0;y<b->body.height;y++) for(uint32_t x=0;x<b->body.width;x++) {
                uint32_t cell=y*16u+x,word=10u+cell/8u;shape[word]|=b->shape.material<<((cell%8u)*4u);
                size_t at=((size_t)bank*allocated+used+word)*8u+cell%8u;
                reactive[at*2u]=handles[n].index;reactive[at*2u+1u]=handles[n].generation;n++;
            }
            b->compound_reserved=absolute;b->motion_flags|=DC_BODY_MOTION_THERMAL_GRID;used+=42u;
        }
        pool[1]=used;pool[2]=geometry_capacity;g->reactive->high_id=identity;
    }
    free(requests);free(handles);return okay||error(err,cap,"Cannot admit constructed member thermal grid");
}
