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
    fr_thermal_inspection_t warmth;
    ASSERT_TRUE(fr_thermal_gpu_inspect(view->gpu,224,271,&warmth,err,sizeof(err)));
    ASSERT_TRUE(warmth.occupied&&warmth.material==DC_MATERIAL_AIR&&warmth.temperature_K>293.16f);
    printf("Measured air warmth %.3f K\n",warmth.temperature_K);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_readback(view->gpu,pixels,480u*270u,err,sizeof(err)));
    uint32_t flame=pixels[207u*480u+160u];
    ASSERT_TRUE((flame&255u)>180u&&((flame>>16)&255u)<100u);
    mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);
    ASSERT_TRUE(image&&SDL_SaveBMP(image,"build/screenshots/ordinary_bonfire.bmp")==0);
    SDL_FreeSurface(image);
    uint32_t *paused=malloc(480u*270u*4u);ASSERT_TRUE(paused);
    ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err))&&dc_gpu_readback(view->gpu,paused,480u*270u,err,sizeof(err)));
    ASSERT_EQ(memcmp(pixels,paused,480u*270u*4u),0);free(pixels);free(paused);
    fr_thermal_probe_t source,restored_source;
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&source,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    view=dc_level_view_create_bonfire(directory,7,err,sizeof(err));ASSERT_TRUE(view);
    ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){2,1},&slot));
    fr_reactive_record_t restored;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,slot,cell,&restored,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&burned,&restored,sizeof(burned)),0);
    ASSERT_TRUE(fr_thermal_gpu_source_state(view->gpu,&restored_source,err,sizeof(err)));
    ASSERT_EQ(memcmp(&source,&restored_source,sizeof(source)),0);
    uint32_t wet_slot;ASSERT_TRUE(dc_chunk_table_find(&view->table,(dc_chunk_coord_t){5,1},&wet_slot));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,cell,&original,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(original.state.bound_water_fp>0);
    ASSERT_TRUE(dc_level_view_heat_probe(view,352,208,true));
    for(uint32_t i=0;i<360;i++) ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err))&&dc_level_view_tick(view,err,sizeof(err)));
    ASSERT_TRUE(dc_level_view_heat_probe(view,352,208,false));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(view->gpu,wet_slot,cell,&restored,&found,err,sizeof(err))&&found);
    ASSERT_EQ(original.state.fuel_mass_fp,restored.state.fuel_mass_fp);
    ASSERT_EQ(restored.state.flags&FR_REACTIVE_FLAMING,0);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));PASS();
}
int main(void) {RUN(test_bonfire_is_a_playable_finite_terrain_scene);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
