#define _POSIX_C_SOURCE 200809L
#define main thermal_bench_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "../../src/app/level.c"
#include "ferrum/fire/thermal_gpu.h"
#include <sys/stat.h>

static bool bench_capture(uint32_t *pixels,const char *path) {
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    bool okay=surface&&SDL_SaveBMP(surface,path)==0;if(surface) SDL_FreeSurface(surface);return okay;
}

static void test_interactive_bench_has_finite_sources_wet_control_probe_and_stable_pause(void) {
    char err[256]={0},directory[]="build/thermal_bench_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_setup_heat_bench(view,err,sizeof(err)));
    uint32_t dry_slot=0,wet_slot=0;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&dry_slot));
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){5,1},&wet_slot));
    fr_reactive_record_t dry,wet;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,dry_slot,0,&dry,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,8,&wet,&found,err,sizeof(err))&&found);
    ASSERT_EQ(dry.state.material,DC_MATERIAL_WOOD);ASSERT_EQ(dry.state.bound_water_fp,0u);ASSERT_TRUE(wet.state.bound_water_fp>0u);
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,true));
    for(uint32_t frame=0;frame<12;frame++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,140,192,false));
    fr_reactive_record_t heated;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,dry_slot,12,&heated,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(heated.state.thermal_energy>dry.state.thermal_energy);
    uint32_t *before=malloc(480u*270u*4u),*after=malloc(480u*270u*4u);ASSERT_TRUE(before&&after);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_pixels(view,before,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);ASSERT_TRUE(bench_capture(before,"build/screenshots/thermal_bench_temperature.bmp"));
    for(uint32_t frame=0;frame<4;frame++) {
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_level_view_pixels(view,after,480u*270u,err,sizeof(err)));
        ASSERT_TRUE(memcmp(before,after,480u*270u*4u)==0);
    }
    ASSERT_TRUE(dc_level_view_heat_overlay(view,2)&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixels(view,after,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(bench_capture(after,"build/screenshots/thermal_bench_wetness.bmp"));
    ASSERT_TRUE(dc_level_view_heat_overlay(view,0)&&dc_level_view_tick(view,err,sizeof(err)));
    free(before);free(after);ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(void) {
    RUN(test_interactive_bench_has_finite_sources_wet_control_probe_and_stable_pause);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
