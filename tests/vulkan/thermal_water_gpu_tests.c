#define main thermal_water_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"

static void test_wetting_moves_exact_water_and_enthalpy_into_wood_before_conduction(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    uint32_t solid=20*64+20,liquid=solid+1;
    c->cells[solid]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    c->cells[liquid]=(dc_cell_t){.material=DC_MATERIAL_WATER,.fluid_mass=65536};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood,water,wet,remaining;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,51,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WATER,65536,0,FR_THERMAL_AMBIENT_K,52,&water,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_absorb_pair(&wood,&water,1.f/60.f,&wet,&remaining,err,sizeof(err)));
    fr_thermal_sample_t a,b;float heat=0;
    ASSERT_TRUE(fr_thermal_sample_from_state(&wet,&a,err,sizeof(err))&&fr_thermal_sample_from_state(&remaining,&b,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_pair_flux(a,b,1.f/60.f,&heat,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,solid,&wood,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err))&&dc_gpu_download_chunk(g,0,c,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,solid,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.bound_water_fp,wet.bound_water_fp);ASSERT_EQ(c->cells[liquid].fluid_mass,remaining.inert_mass_fp);
    ASSERT_EQ(record.state.bound_water_fp+c->cells[liquid].fluid_mass,65536u);
    ASSERT_TRUE(fabsf(record.state.thermal_energy-(wet.thermal_energy-heat))<1e-4f);
    float energy[4096];ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,energy,4096,err,sizeof(err)));
    ASSERT_TRUE(fabsf(energy[liquid]-heat)<1e-4f);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_actual_parallel_water_transport_preserves_excess_heat_and_custom_pigment(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(c&&colors);
    /* Runtime liquid volume occupies permeable AIR cells; material WATER is
     * an authoring/brush command, not the post-import substrate. */
    uint32_t source=12*64+20;c->cells[source]=(dc_cell_t){.material=DC_MATERIAL_AIR,.fluid_mass=65536};c->face_velocity[source].y=1.f;
    float pigment[4]={.7f,.15f,.4f,1.f};memcpy(colors->water[source],pigment,sizeof(pigment));
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_renderer_upload_chunk_colors(g,0,colors,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_seed_water(g,0,source,100.f,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(g,1.f/60.f)&&dc_gpu_set_fluid_interval(g,2)&&dc_gpu_set_rigid_solver(g,true));
    for(uint32_t i=0;i<8;i++) ASSERT_TRUE(dc_gpu_present_chunks_steps(g,1,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,c,err,sizeof(err))&&fr_renderer_download_chunk_colors(g,0,colors,err,sizeof(err)));
    float energy[4096];ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,energy,4096,err,sizeof(err)));
    uint64_t mass=0;double heat=0;bool moved=false;
    for(uint32_t i=0;i<4096;i++) {
        mass+=c->cells[i].fluid_mass;heat+=energy[i];
        if(c->cells[i].fluid_mass) {
            moved|=i/64>12;
            for(uint32_t channel=0;channel<4;channel++) ASSERT_TRUE(fabsf(colors->water[i][channel]-pigment[channel])<2e-5f);
        }
    }
    ASSERT_EQ(mass,65536u);ASSERT_TRUE(moved);ASSERT_TRUE(fabs(heat-100.)<.02);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(colors);free(c);dc_gpu_destroy(g);PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_wetting_moves_exact_water_and_enthalpy_into_wood_before_conduction);
    RUN(test_actual_parallel_water_transport_preserves_excess_heat_and_custom_pigment);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
