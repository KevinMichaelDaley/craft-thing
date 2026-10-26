#define _POSIX_C_SOURCE 200809L
#define main bonfire_fixture_main
#include "../vulkan/reactive_cell_gpu_tests.c"
#undef main
#include "../../src/app/level.c"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include <sys/stat.h>
static void test_bonfire_is_a_playable_finite_terrain_scene(void) {
    char err[256]={0},directory[]="build/bonfire_display_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view=dc_level_view_create_bonfire(directory,7,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(fr_combustion_gpu_enabled(view->gpu));
    uint32_t slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    uint32_t cell=16u*64u+32u;fr_reactive_record_t original;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&original,&found,err,sizeof(err))&&found);
    ASSERT_EQ(original.state.material,DC_MATERIAL_WOOD);ASSERT_TRUE(original.state.fuel_mass_fp>0);
    ASSERT_TRUE(dc_level_view_heat_probe(view,160,208,true));
    for(uint32_t i=0;i<360;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,160,208,false));
    fr_reactive_record_t burned;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&burned,&found,err,sizeof(err))&&found);
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&burned.state,&temperature,err,sizeof(err)));
    printf("Bonfire contact: temperature %.3f K fuel %u -> %u hazard %.5f / %.5f\n",temperature,
        original.state.fuel_mass_fp,burned.state.fuel_mass_fp,burned.state.ignition_integral,burned.state.ignition_threshold);
    ASSERT_TRUE(burned.state.fuel_mass_fp<original.state.fuel_mass_fp);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_readback(view->gpu,pixels,480u*270u,err,sizeof(err)));
    uint32_t flame=pixels[207u*480u+160u];
    ASSERT_TRUE((flame&255u)>180u&&((flame>>16)&255u)<100u);
    mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    ASSERT_TRUE(image&&SDL_SaveBMP(image,"build/screenshots/ordinary_bonfire.bmp")==0);
    SDL_FreeSurface(image);free(pixels);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(void) {RUN(test_bonfire_is_a_playable_finite_terrain_scene);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
