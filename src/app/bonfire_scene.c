#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "ferrum/fire/material_profile.h"
#include "bonfire.h"
typedef struct {
    const dc_chunk_coord_t *coords;dc_thermal_bench_chunk_t *chunks;
    uint32_t count;atomic_uint next;dc_bonfire_plan_t plan;
} builder_t;
static uint32_t material_at(const builder_t *b,int64_t x,int64_t y,const dc_bonfire_piece_t **piece) {
    *piece=NULL;
    if(y>=96) return DC_MATERIAL_STONE;
    for(uint32_t i=0;i<b->plan.count;i++) {
        const dc_bonfire_piece_t *p=b->plan.pieces+i;
        if(x>=p->x&&x<p->x+p->width&&y>=p->y&&y<p->y+p->height) {*piece=p;return DC_MATERIAL_WOOD;}
    }
    return DC_MATERIAL_AIR;
}
static int worker(void *data) {
    builder_t *b=data;
    for(uint32_t index=atomic_fetch_add(&b->next,1u);index<b->count;index=atomic_fetch_add(&b->next,1u)) {
        dc_thermal_bench_chunk_t *out=b->chunks+index;
        out->chunk=calloc(1,sizeof(*out->chunk));out->colors=calloc(1,sizeof(*out->colors));
        out->reactive.attachments=calloc(4096,sizeof(*out->reactive.attachments));
        if(!out->chunk||!out->colors||!out->reactive.attachments) {
            snprintf(out->error,sizeof(out->error),"Cannot allocate finite bonfire fixture");continue;
        }
        out->chunk->coord=b->coords[index];out->reactive.next_id=(uint64_t)b->count*4096+1;out->valid=true;
        for(uint32_t i=0;i<4096&&out->valid;i++) {
            int64_t x=out->chunk->coord.x*64+i%64,y=out->chunk->coord.y*64+i/64;
            const dc_bonfire_piece_t *piece=NULL;uint32_t material=material_at(b,x,y,&piece);if(!material) continue;
            out->chunk->cells[i]=(dc_cell_t){.material=material,.flags=piece?0:DC_CELL_LOADED_TERRAIN};
            const fr_reactive_profile_t *p=fr_reactive_profile(material);
            uint32_t mass=(uint32_t)llround(p->density*65536.),water=0;
            if(piece&&piece->wet&&piece->kind!=DC_BONFIRE_STARTER) water=(uint32_t)llround(mass*p->moisture_capacity);
            fr_reactive_attachment_t *a=out->reactive.attachments+out->reactive.count++;
            a->local_index=i;a->record.owner=fr_reactive_cell_owner(out->chunk->coord,i);
            out->valid=fr_reactive_state_default(material,mass,water,piece?piece->temperature_K:293.15f,1+(uint64_t)index*4096+i,
                &a->record.state,out->error,sizeof(out->error));
            if(out->valid&&piece&&piece->kind==DC_BONFIRE_STARTER) {
                fr_reactive_state_t *state=&a->record.state;
                state->char_mass_fp=state->fuel_mass_fp;state->fuel_mass_fp=0;
                state->flags|=FR_REACTIVE_FLAMING;
                state->thermal_energy=(float)(fr_reactive_state_heat_capacity(state)*piece->temperature_K);
                out->valid=fr_reactive_state_valid(state,out->error,sizeof(out->error));
            }
            float grain=(float)(((uint64_t)x*3+(uint64_t)y*11)&15)/255.f;
            float *color=out->colors->terrain[i];
            bool organic=material==DC_MATERIAL_WOOD||material==DC_MATERIAL_LEAVES;
            color[0]=(organic?.35f:.23f)+grain;
            color[1]=(organic?.19f:.25f)+grain;
            color[2]=(organic?.08f:.28f)+grain;color[3]=1;
        }
    }
    return 0;
}
bool dc_bonfire_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_thermal_bench_chunk_t *out,char *err,uint32_t cap) {
    if(!coords||!count||!out) return false;
    builder_t b={.coords=coords,.chunks=out,.count=count};atomic_init(&b.next,0);
    if(!dc_bonfire_plan(&b.plan)) return false;
    thrd_t threads[3];uint32_t started=0;
    for(uint32_t i=0;i<3;i++) if(thrd_create(threads+started,worker,&b)==thrd_success) started++;
    worker(&b);for(uint32_t i=0;i<started;i++) thrd_join(threads[i],NULL);
    for(uint32_t i=0;i<count;i++) if(!out[i].valid) {
        if(err&&cap) snprintf(err,cap,"%s",out[i].error);
        return false;
    }
    return true;
}
