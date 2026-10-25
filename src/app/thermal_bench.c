#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal.h"
#include "thermal_bench.h"

typedef struct {
    const dc_chunk_coord_t *coords;
    dc_thermal_bench_chunk_t *chunks;
    uint32_t count;
    atomic_uint next;
} build_t;
static uint32_t material_at(int64_t x,int64_t y) {
    if(y>=96) return DC_MATERIAL_STONE;
    for(int64_t center=120;center<=320;center+=200) {
        if(x>=center&&x<center+8&&y>=64) return DC_MATERIAL_STONE;
        if(x>=center+8&&x<center+32&&y>=64&&y<72) return DC_MATERIAL_WOOD;
        if(((x>=center-24&&x<center-20)||(x>=center+56&&x<center+60))&&y>=40) return DC_MATERIAL_STONE;
    }
    return DC_MATERIAL_AIR;
}
static int build_worker(void *data) {
    build_t *build=data;
    for(uint32_t index=atomic_fetch_add(&build->next,1u);index<build->count;index=atomic_fetch_add(&build->next,1u)) {
        dc_thermal_bench_chunk_t *out=build->chunks+index;
        out->chunk=calloc(1,sizeof(*out->chunk));out->colors=calloc(1,sizeof(*out->colors));
        out->reactive.attachments=calloc(DC_CHUNK_CELLS,sizeof(*out->reactive.attachments));
        if(!out->chunk||!out->colors||!out->reactive.attachments) {snprintf(out->error,sizeof(out->error),"Cannot allocate heat bench fixture");continue;}
        out->chunk->coord=build->coords[index];out->reactive.next_id=(uint64_t)build->count*DC_CHUNK_CELLS+1u;out->valid=true;
        for(uint32_t cell=0;out->valid&&cell<DC_CHUNK_CELLS;cell++) {
            int64_t x=out->chunk->coord.x*64+(cell%64u),y=out->chunk->coord.y*64+(cell/64u);
            uint32_t material=material_at(x,y);if(material==DC_MATERIAL_AIR) continue;
            out->chunk->cells[cell]=(dc_cell_t){.material=material,.flags=DC_CELL_LOADED_TERRAIN};
            const fr_reactive_profile_t *profile=fr_reactive_profile(material);
            uint32_t mass=(uint32_t)llround((double)profile->density*DC_FLUID_FULL);
            bool hot=material==DC_MATERIAL_STONE&&y>=64&&y<72&&((x>=120&&x<128)||(x>=320&&x<328));
            bool wet=material==DC_MATERIAL_WOOD&&x>=328&&x<352;
            uint32_t water=wet?(uint32_t)llround((double)mass*profile->moisture_capacity):0;
            fr_reactive_attachment_t *attachment=out->reactive.attachments+out->reactive.count++;
            attachment->local_index=cell;attachment->record.owner=fr_reactive_cell_owner(out->chunk->coord,cell);
            out->valid=fr_reactive_state_default(material,mass,water,hot?FR_THERMAL_PROBE_K:FR_THERMAL_AMBIENT_K,
                1u+(uint64_t)index*DC_CHUNK_CELLS+cell,&attachment->record.state,out->error,sizeof(out->error));
            float grain=(float)(((uint64_t)x*7u+(uint64_t)y*3u)&15u)/255.f;
            float *color=out->colors->terrain[cell];
            color[0]=(material==DC_MATERIAL_WOOD?.32f:.25f)+grain;
            color[1]=(material==DC_MATERIAL_WOOD?.15f:.27f)+grain;
            color[2]=(material==DC_MATERIAL_WOOD?.06f:.29f)+grain;color[3]=1.f;
        }
    }
    return 0;
}
bool dc_thermal_bench_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_thermal_bench_chunk_t *out,char *err,uint32_t cap) {
    if(!coords||!count||!out) return false;
    build_t build={.coords=coords,.chunks=out,.count=count};atomic_init(&build.next,0);
    thrd_t workers[3];uint32_t started=0;
    for(uint32_t i=0;i<3;i++) if(thrd_create(workers+started,build_worker,&build)==thrd_success) started++;
    build_worker(&build);for(uint32_t i=0;i<started;i++) thrd_join(workers[i],NULL);
    for(uint32_t i=0;i<count;i++) if(!out[i].valid) {if(err&&cap) snprintf(err,cap,"%s",out[i].error);return false;}
    return true;
}
void dc_thermal_bench_release(dc_thermal_bench_chunk_t *chunks,uint32_t count) {
    if(!chunks) return;
    for(uint32_t i=0;i<count;i++) {free(chunks[i].chunk);free(chunks[i].colors);fr_reactive_chunk_release(&chunks[i].reactive);}
    free(chunks);
}
