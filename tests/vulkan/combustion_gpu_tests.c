#define main combustion_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"

static void test_displayed_terrain_combustion_debits_real_fuel_and_oxygen(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,100,&wood,err,sizeof(err)));
    wood.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *before=calloc(4096,sizeof(*before)),*after=calloc(4096,sizeof(*after));ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp<wood.fuel_mass_fp&&result.state.char_mass_fp>0);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    double oxygen=0,products=0;
    for(uint32_t i=0;i<4096;i++) {
        oxygen+=before[i].species[FR_GAS_OXYGEN]-after[i].species[FR_GAS_OXYGEN];
        products+=after[i].species[FR_GAS_EXHAUST]+after[i].species[FR_GAS_SOOT];
    }
    ASSERT_TRUE(oxygen>0&&products>oxygen);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    fr_reactive_record_t paused;ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&paused,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&result,&paused,sizeof(result)),0);
    free(before);free(after);free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_displayed_terrain_combustion_debits_real_fuel_and_oxygen);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
