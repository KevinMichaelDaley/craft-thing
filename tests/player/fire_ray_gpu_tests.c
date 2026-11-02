#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/player/fire_ray_gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static int g_pass,g_fail;
static unsigned g_submits;
bool __real_fr_renderer_submit(dc_gpu_t *gpu);
bool __wrap_fr_renderer_submit(dc_gpu_t *gpu) {g_submits++;return __real_fr_renderer_submit(gpu);}
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,err);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static dc_gpu_t *g;static char err[256];static fr_player_body_binding_t binding;
static bool fixture(bool left) {
    dc_chunk_t *c=calloc(1,sizeof(*c));if(!c) return false;
    bool okay=dc_gpu_create(&g,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err));
    for(uint32_t slot=0;slot<2&&okay;slot++) {
        c->coord=(dc_chunk_coord_t){slot,0};
        for(uint32_t p=0;p<4096;p++) c->cells[p]=(dc_cell_t){0};
        if(slot==0) for(uint32_t y=20;y<=24;y++) {
            c->cells[y*64+28]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
            c->cells[y*64+60]=(dc_cell_t){.material=DC_MATERIAL_DENSE_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
            c->cells[y*64+44]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
        }
        okay=dc_gpu_upload_chunk(g,slot,c,err,sizeof(err))&&dc_gpu_set_page(g,slot,0,slot,err,sizeof(err));
    }
    free(c);
    for(uint32_t n=0;n<10&&okay;n++) {
        uint32_t y=20+n/2,x=n%2?60:28,m=n%2?DC_MATERIAL_DENSE_WOOD:DC_MATERIAL_WOOD;
        fr_reactive_state_t state;
        okay=fr_reactive_state_default(m,(uint32_t)lroundf(fr_reactive_profile(m)->density*65536.f),0,293.15f,n+1,&state,err,sizeof(err))&&
            fr_gpu_reactive_seed_cell(g,0,y*64+x,&state,err,sizeof(err));
    }
    dc_gpu_world_body_t body={.chunk={left?1:0,0},.body={.id=71,.active=1,.width=4,.height=4,.x_fp=(left?26:8)*65536,.y_fp=20*65536}};
    okay=okay&&dc_gpu_spawn_world_body(g,body,err,sizeof(err))&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err));
    binding=(fr_player_body_binding_t){.identity=1,.body_id=71};
    for(uint32_t s=0;s<dc_gpu_body_capacity(g);s++) {
        dc_body_storage_owner_t *o=dc_gpu_body_owner(g,s);if(o->runtime_id==71) binding.generation=o->generation;
    }
    return okay&&binding.generation;
}
static float shock(uint32_t x) {
    fr_reactive_record_t r;bool found=false;
    if(!fr_gpu_reactive_read_cell(g,0,22*64+x,&r,&found,err,sizeof(err))||!found) return -1;
    return r.state.pending_shock_energy;
}
static void test_facing_ray_heats_real_fuel_and_stops_at_stone(void) {
    for(unsigned left=0;left<2;left++) {
        ASSERT_TRUE(fixture(left!=0));fr_fire_ray_gpu_t *r=NULL;
        ASSERT_TRUE(fr_fire_ray_gpu_create(g,&binding,1,(fr_fire_ray_config_t){128,1,.3f,1800},&r,err,sizeof(err)));
        fr_fire_ray_input_t input={1,1,left?-1:1,false};
        ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));ASSERT_EQ(shock(28),0);ASSERT_EQ(shock(60),0);
        input.sequence=2;input.emit=true;
        ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));
        ASSERT_TRUE(shock(left?60:28)>0);ASSERT_EQ(shock(left?28:60),0);
        fr_fire_ray_stats_t stats;ASSERT_TRUE(fr_fire_ray_gpu_stats(r,0,&stats));ASSERT_EQ(stats.casts,1);ASSERT_TRUE(stats.blocked_rows>0);
        fr_fire_ray_gpu_destroy(r);dc_gpu_destroy(g);
    }
    PASS();
}
static void test_retries_and_ticks_do_not_start_another_cast(void) {
    ASSERT_TRUE(fixture(false));fr_fire_ray_gpu_t *r=NULL;
    ASSERT_TRUE(fr_fire_ray_gpu_create(g,&binding,1,(fr_fire_ray_config_t){128,1,.1f,1400},&r,err,sizeof(err)));
    fr_fire_ray_input_t input={1,1,1,true};
    for(unsigned n=0;n<30;n++) ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));
    fr_fire_ray_stats_t stats;ASSERT_TRUE(fr_fire_ray_gpu_stats(r,0,&stats));ASSERT_EQ(stats.casts,1);ASSERT_EQ(stats.remaining,0);
    input.emit=false;input.sequence=2;ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_TRUE(fr_fire_ray_gpu_stats(r,0,&stats));ASSERT_EQ(stats.casts,1);
    input.sequence=3;input.emit=true;ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));
    ASSERT_TRUE(fr_fire_ray_gpu_stats(r,0,&stats));ASSERT_EQ(stats.casts,2);
    fr_fire_ray_gpu_destroy(r);dc_gpu_destroy(g);PASS();
}
static void test_idle_ray_does_not_submit_gpu_work(void) {
    ASSERT_TRUE(fixture(false));fr_fire_ray_gpu_t *r=NULL;
    ASSERT_TRUE(fr_fire_ray_gpu_create(g,&binding,1,(fr_fire_ray_config_t){128,1,.1f,1400},&r,err,sizeof(err)));
    unsigned before=g_submits;fr_fire_ray_input_t input={1,1,1,false};
    for(unsigned n=0;n<10;n++) {input.sequence++;ASSERT_TRUE(fr_fire_ray_gpu_submit(r,&input,1,1.f/60,err,sizeof(err)));}
    ASSERT_EQ(g_submits,before);
    fr_fire_ray_gpu_destroy(r);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_idle_ray_does_not_submit_gpu_work);RUN(test_facing_ray_heats_real_fuel_and_stops_at_stone);RUN(test_retries_and_ticks_do_not_start_another_cast);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
