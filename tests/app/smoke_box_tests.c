#define _POSIX_C_SOURCE 200809L
#define main smoke_box_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "../../src/app/level.c"
#include "ferrum/fire/gas_gpu.h"

static bool smoke_budget(dc_level_view_t *view,double species[5],double *energy,double pigment[3],double *work,double *height,
    char *err,uint32_t cap) {
    fr_gas_state_t *states=malloc(4096*sizeof(*states));double *external=malloc(4096*sizeof(*external));if(!states||!external) {free(states);free(external);return false;}
    bool okay=true;memset(species,0,5*sizeof(*species));memset(pigment,0,3*sizeof(*pigment));*energy=0;*work=0;*height=0;
    for(uint32_t slot=0;okay&&slot<view->table.capacity;slot++) {
        okay=fr_gas_gpu_read_chunk(view->gpu,slot,states,4096,err,cap)&&fr_gas_gpu_read_work_chunk(view->gpu,slot,external,4096,err,cap);
        for(uint32_t i=0;okay&&i<4096;i++) {
            for(uint32_t k=0;k<5;k++) species[k]+=states[i].species[k];
            for(uint32_t k=0;k<3;k++) pigment[k]+=states[i].pigment[k];
            *energy+=states[i].energy;*work+=external[i];
            *height+=states[i].pigment[0]*(view->table.slots[slot].coord.y*64+(int64_t)(i/64u));
        }
    }
    free(states);free(external);return okay;
}
static void test_native_placement_box_has_finite_sources_real_controls_rising_smoke_and_stable_pause(void) {
    char err[256]={0},directory[]="build/smoke_box_native_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_smoke_box(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_setup_smoke_box(view,err,sizeof(err)));
    uint32_t *first=malloc(480u*270u*4u),*second=malloc(480u*270u*4u);ASSERT_TRUE(first&&second);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,first,480u*270u,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&before)&&before.physical_steps==0u);
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));ASSERT_EQ(memcmp(first,second,480u*270u*4u),0);
    ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&after));ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    double stock[5],pigment[3],energy,work,height,moved[5],moved_pigment[3],moved_energy,moved_work,moved_height;
    ASSERT_TRUE(smoke_budget(view,stock,&energy,pigment,&work,&height,err,sizeof(err)));
    ASSERT_TRUE(stock[2]>0&&stock[2]<4&&pigment[0]>0&&pigment[2]>0);
    ASSERT_TRUE(dc_level_view_smoke_valve(view,0,err,sizeof(err))&&dc_level_view_smoke_valve(view,1,err,sizeof(err))&&dc_level_view_smoke_valve(view,2,err,sizeof(err)));
    for(uint32_t i=0;i<30;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_clock(view->gpu,&after));ASSERT_EQ(after.physical_steps,30u);ASSERT_TRUE(fabs(after.advanced_seconds-.5)<1e-7);
    ASSERT_TRUE(smoke_budget(view,moved,&moved_energy,moved_pigment,&moved_work,&moved_height,err,sizeof(err)));
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(moved[k]-stock[k])<2e-5);
    for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(moved_pigment[k]-pigment[k])<2e-5);
    ASSERT_TRUE(fabs((moved_energy-energy)-(moved_work-work))<.02);
    ASSERT_TRUE(moved_height<height-.01);
    ASSERT_TRUE(dc_level_view_smoke_overlay(view,1)&&dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(memcmp(first,second,480u*270u*4u)!=0);mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(second,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/smoke_box_oxygen.bmp"),0);SDL_FreeSurface(image);
    ASSERT_TRUE(dc_level_view_smoke_overlay(view,0)&&dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,second,480u*270u,err,sizeof(err)));
    image=SDL_CreateRGBSurfaceWithFormatFrom(second,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/smoke_box_color.bmp"),0);SDL_FreeSurface(image);
    free(first);free(second);ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);RUN(test_native_placement_box_has_finite_sources_real_controls_rising_smoke_and_stable_pause);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
