#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "smoke_box.h"

typedef struct {
    const dc_chunk_coord_t *coords;
    dc_smoke_box_chunk_t *chunks;
    fr_gas_state_t air,source[2];
    uint32_t count;
    atomic_uint next;
} build_t;
fr_gas_model_t dc_smoke_box_model(void) {
    fr_gas_model_t m={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    for(uint32_t i=0;i<5;i++) {m.gas_constant[i]=(float)m.gas_constant[i];m.heat_capacity_cv[i]=(float)m.heat_capacity_cv[i];}
    return m;
}
static bool rectangle(int64_t x,int64_t y,int64_t left,int64_t top,int64_t right,int64_t bottom) {
    return x>=left&&x<right&&y>=top&&y<bottom;
}
static bool wall(int64_t x,int64_t y,int64_t left,int64_t top,int64_t right,int64_t bottom) {
    return rectangle(x,y,left,top,right,bottom)&&!rectangle(x,y,left+4,top+4,right-4,bottom-4);
}
static bool stone(int64_t x,int64_t y) {
    if(y>=96) return true;
    if(rectangle(x,y,124,40,132,56)||rectangle(x,y,324,40,332,56)||rectangle(x,y,392,-48,404,-32)) return false;
    if(wall(x,y,80,-80,400,48)||wall(x,y,104,48,160,88)||wall(x,y,304,48,360,88)) return true;
    if(rectangle(x,y,80,44,84,96)||rectangle(x,y,396,44,400,96)) return true;
    for(int64_t left=104;left<=304;left+=200)
        if(rectangle(x,y,left,84,left+4,96)||rectangle(x,y,left+52,84,left+56,96)) return true;
    return false;
}
static int reservoir(int64_t x,int64_t y) {
    for(int i=0;i<2;i++) {
        int64_t left=104+i*200;
        if(rectangle(x,y,left+4,52,left+52,84)||rectangle(x,y,left+20,48,left+28,56)) return i;
    }
    return -1;
}
static int worker(void *data) {
    build_t *build=data;
    for(uint32_t index=atomic_fetch_add(&build->next,1u);index<build->count;index=atomic_fetch_add(&build->next,1u)) {
        dc_smoke_box_chunk_t *out=build->chunks+index;
        out->chunk=calloc(1,sizeof(*out->chunk));out->colors=calloc(1,sizeof(*out->colors));
        out->gas=calloc(4096,sizeof(*out->gas));out->void_fractions=calloc(4096,sizeof(*out->void_fractions));
        if(!out->chunk||!out->colors||!out->gas||!out->void_fractions) {snprintf(out->error,sizeof(out->error),"Cannot allocate smoke scene chunk");continue;}
        out->chunk->coord=build->coords[index];
        for(uint32_t i=0;i<4096;i++) {
            int64_t x=out->chunk->coord.x*64+i%64u,y=out->chunk->coord.y*64+i/64u;
            if(stone(x,y)) {
                out->chunk->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
                float grain=(float)(((uint64_t)x*7u+(uint64_t)y*3u)&15u)/255.f;
                float *color=out->colors->terrain[i];color[0]=.22f+grain;color[1]=.25f+grain;color[2]=.29f+grain;color[3]=1;
            } else {
                int source=reservoir(x,y);out->void_fractions[i]=1;out->gas[i]=source<0?build->air:build->source[source];
            }
        }
        out->valid=true;
    }
    return 0;
}
bool dc_smoke_box_build(const dc_chunk_coord_t *coords,uint32_t count,
    dc_smoke_box_chunk_t *out,char *err,uint32_t cap) {
    if(!coords||!count||!out) {if(err&&cap) snprintf(err,cap,"Invalid smoke fixture request");return false;}
    fr_gas_model_t model=dc_smoke_box_model();float fraction=.21f;
    float oxygen=(fraction/(float)model.gas_constant[0])/(fraction/(float)model.gas_constant[0]+(1.f-fraction)/(float)model.gas_constant[1]);
    float mass=.0012f,m0=mass*oxygen,m1=mass-m0;
    float capacity=m0*(float)model.heat_capacity_cv[0]+m1*(float)model.heat_capacity_cv[1];
    build_t build={.coords=coords,.chunks=out,.count=count};atomic_init(&build.next,0);
    build.air.species[0]=m0;build.air.species[1]=m1;build.air.energy=capacity*293.15f;
    fr_gas_thermo_t ambient;
    if(!fr_gas_thermodynamics(&model,&build.air,1,&ambient,err,cap)) return false;
    float smoke_mass=(float)(ambient.pressure*model.kinetic_scale/(model.gas_constant[2]*650.));
    const double colors[2][3]={{.85,.15,.03},{.08,.3,.95}};
    for(uint32_t i=0;i<2;i++) {
        build.source[i].species[2]=smoke_mass;build.source[i].energy=smoke_mass*model.heat_capacity_cv[2]*650.;
        for(uint32_t k=0;k<3;k++) build.source[i].pigment[k]=smoke_mass*colors[i][k];
    }
    thrd_t threads[3];uint32_t started=0;
    for(uint32_t i=0;i<3;i++) if(thrd_create(threads+started,worker,&build)==thrd_success) started++;
    worker(&build);for(uint32_t i=0;i<started;i++) thrd_join(threads[i],NULL);
    for(uint32_t i=0;i<count;i++) if(!out[i].valid) {if(err&&cap) snprintf(err,cap,"%s",out[i].error);return false;}
    return true;
}
void dc_smoke_box_release(dc_smoke_box_chunk_t *chunks,uint32_t count) {
    if(!chunks) return;
    for(uint32_t i=0;i<count;i++) {free(chunks[i].chunk);free(chunks[i].colors);free(chunks[i].gas);free(chunks[i].void_fractions);chunks[i]=(dc_smoke_box_chunk_t){0};}
}
