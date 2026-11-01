#define main reactive_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"

static void test_wet_wood_dries_into_the_local_gas_without_losing_fuel(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,450,101,&wood,err,sizeof(err)));
    wood.ignition_threshold=1e9f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *before=calloc(4096,sizeof(*before)),*after=calloc(4096,sizeof(*after));ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err)));
    for(uint32_t i=0;i<24;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    double vapor=0;for(uint32_t i=0;i<4096;i++) vapor+=after[i].species[FR_GAS_VAPOR]-before[i].species[FR_GAS_VAPOR];
    uint32_t dried=wood.bound_water_fp-record.state.bound_water_fp;
    ASSERT_TRUE(dried>0u);
    ASSERT_TRUE(fabs(vapor-(double)dried/65536.)<1e-6);
    ASSERT_EQ(record.state.fuel_mass_fp,wood.fuel_mass_fp);
    ASSERT_TRUE(record.state.evaporation_fraction>=0.f&&record.state.evaporation_fraction<1.f);
    ASSERT_TRUE(fr_combustion_gpu_step(g,0,err,sizeof(err)));
    fr_reactive_record_t paused;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&paused,&found,err,sizeof(err))&&found);
    ASSERT_EQ(paused.state.bound_water_fp,record.state.bound_water_fp);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    free(before);free(after);free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_wet_wood_dries_into_the_local_gas_without_losing_fuel);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
