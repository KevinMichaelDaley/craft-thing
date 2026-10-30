#define main thermal_gpu_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "ferrum/fire/thermal_gpu.h"
#include "../../src/vulkan/thermal_gpu_state.h"

static bool setup_pair(dc_gpu_t **g,fr_reactive_state_t *stone,fr_reactive_state_t *wood,char *err,uint32_t cap) {
    dc_chunk_t *c=calloc(1,sizeof(*c));if(!c) return false;
    c->cells[20*64+20]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    c->cells[20*64+21]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    bool okay=context(g,err,cap)&&dc_gpu_upload_chunk(*g,0,c,err,cap)&&dc_gpu_set_page(*g,0,0,0,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,350,41,stone,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,293.15f,42,wood,err,cap)&&
        fr_gpu_reactive_seed_cell(*g,0,20*64+20,stone,err,cap)&&fr_gpu_reactive_seed_cell(*g,0,20*64+21,wood,err,cap);
    free(c);return okay;
}
static void test_thermal_work_grids_are_local_and_inspection_stays_exact(void) {
    char err[256]={0};dc_gpu_t *g=NULL;fr_reactive_state_t stone,wood;
    ASSERT_TRUE(setup_pair(&g,&stone,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(g->thermal->samples.device_local&&!g->thermal->samples.host_visible);
    ASSERT_TRUE(g->thermal->fluxes.device_local&&!g->thermal->fluxes.host_visible);
    fr_thermal_inspection_t info={0};
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,20,20,&info,err,sizeof(err)));
    ASSERT_TRUE(info.occupied&&fabsf(info.temperature_K-350.f)<.001f);
    dc_gpu_destroy(g);PASS();
}
static void test_vulkan_face_exchange_matches_reference_and_paused_presentation_freezes_energy(void) {
    char err[256]={0};dc_gpu_t *g=NULL;fr_reactive_state_t stone,wood;
    ASSERT_TRUE(setup_pair(&g,&stone,&wood,err,sizeof(err)));
    fr_thermal_sample_t a,b;float flux=0;
    ASSERT_TRUE(fr_thermal_sample_from_state(&stone,&a,err,sizeof(err))&&fr_thermal_sample_from_state(&wood,&b,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_pair_flux(a,b,1.f/60.f,&flux,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    fr_reactive_record_t hot,cold;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&hot,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+21,&cold,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fabsf(hot.state.thermal_energy-(stone.thermal_energy-flux))<1e-4f);
    ASSERT_TRUE(fabsf(cold.state.thermal_energy-(wood.thermal_energy+flux))<1e-4f);
    ASSERT_TRUE(fabs((double)hot.state.thermal_energy+cold.state.thermal_energy-stone.thermal_energy-wood.thermal_energy)<1e-4);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    fr_reactive_record_t paused;ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&paused,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&paused,&hot,sizeof(hot))==0);
#ifdef FR_REACTIVE_PRESENTATION_TEST
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(surface);
    ASSERT_EQ(SDL_SaveBMP(surface,"build/screenshots/thermal_reference_pair.bmp"),0);SDL_FreeSurface(surface);free(pixels);
#endif
    dc_gpu_destroy(g);PASS();
}
static void test_real_physics_tick_advances_heat_once_despite_rigid_and_mpm_substeps(void) {
    char err[256]={0};dc_gpu_t *g=NULL;fr_reactive_state_t stone,wood;
    ASSERT_TRUE(setup_pair(&g,&stone,&wood,err,sizeof(err)));
    fr_thermal_sample_t a,b;float flux=0;
    ASSERT_TRUE(fr_thermal_sample_from_state(&stone,&a,err,sizeof(err))&&fr_thermal_sample_from_state(&wood,&b,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_pair_flux(a,b,1.f/60.f,&flux,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&dc_gpu_set_tick_seconds(g,1.f/60.f)&&dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    dc_gpu_rigid_solver_stats_t stats;ASSERT_TRUE(dc_gpu_read_rigid_solver_stats(g,&stats));ASSERT_EQ(stats.iterations,8u);ASSERT_EQ(stats.substeps,8u);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+21,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fabsf(record.state.thermal_energy-(wood.thermal_energy+flux))<1e-4f);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_thermal_work_grids_are_local_and_inspection_stays_exact);
    RUN(test_vulkan_face_exchange_matches_reference_and_paused_presentation_freezes_energy);
    RUN(test_real_physics_tick_advances_heat_once_despite_rigid_and_mpm_substeps);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
