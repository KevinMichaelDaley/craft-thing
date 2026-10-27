#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "ferrum/fire/material_profile.h"
#include "bonfire.h"
typedef struct {
    const dc_chunk_coord_t *coords;dc_thermal_bench_chunk_t *chunks;
    uint32_t count;atomic_uint next;
} builder_t;
static uint32_t material_at(int64_t x,int64_t y) {
    if(y>=96) return DC_MATERIAL_STONE;
    for(int64_t center=160;center<=352;center+=192) {
        int64_t dx=x-center;
        if(y>=92&&dx>=-28&&dx<28) return DC_MATERIAL_STONE;
        if(y>=80&&((dx>=-28&&dx<-24)||(dx>=24&&dx<28))) return DC_MATERIAL_STONE;
        if(y>=90&&y<92&&dx>=-1&&dx<=1) return DC_MATERIAL_CHAR;
        if((y==89||y==90)&&(dx==-2||dx==2)) return DC_MATERIAL_LEAVES;
        if(y==91&&((dx>=-3&&dx<=-2)||(dx>=2&&dx<=3))) return DC_MATERIAL_WOOD;
        if(y>=80&&y<90&&dx>=-3&&dx<=3) return DC_MATERIAL_AIR;
        if(y>=90&&y<92&&(dx==-2||dx==2)) return DC_MATERIAL_AIR;
        if(y>=90&&y<92&&dx>=-6&&dx<=6&&(dx%2)==0) return DC_MATERIAL_WOOD;
        if((y>=88&&y<90&&dx>=-20&&dx<20)||
           (y>=84&&y<86&&dx>=-16&&dx<16)||
           (y>=80&&y<82&&dx>=-12&&dx<12)||
           (y>=82&&y<88&&(dx==-4||dx==4))) return DC_MATERIAL_WOOD;
        if(y>=84&&dx>=52&&dx<56) return DC_MATERIAL_WOOD;
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
            uint32_t material=material_at(x,y);if(!material) continue;
            out->chunk->cells[i]=(dc_cell_t){.material=material,.flags=DC_CELL_LOADED_TERRAIN};
            const fr_reactive_profile_t *p=fr_reactive_profile(material);
            uint32_t mass=(uint32_t)llround(p->density*65536.),water=0;
            if((material==DC_MATERIAL_WOOD||material==DC_MATERIAL_LEAVES)&&x>=300) water=(uint32_t)llround(mass*p->moisture_capacity);
            fr_reactive_attachment_t *a=out->reactive.attachments+out->reactive.count++;
            a->local_index=i;a->record.owner=fr_reactive_cell_owner(out->chunk->coord,i);
            out->valid=fr_reactive_state_default(material,mass,water,material==DC_MATERIAL_CHAR?1400.f:293.15f,1+(uint64_t)index*4096+i,
                &a->record.state,out->error,sizeof(out->error));
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
    thrd_t threads[3];uint32_t started=0;
    for(uint32_t i=0;i<3;i++) if(thrd_create(threads+started,worker,&b)==thrd_success) started++;
    worker(&b);for(uint32_t i=0;i<started;i++) thrd_join(threads[i],NULL);
    for(uint32_t i=0;i<count;i++) if(!out[i].valid) {
        if(err&&cap) snprintf(err,cap,"%s",out[i].error);
        return false;
    }
    return true;
}
