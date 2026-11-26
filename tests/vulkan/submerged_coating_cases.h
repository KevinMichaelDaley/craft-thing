#ifndef FERRUM_TESTS_SUBMERGED_COATING_CASES_H
#define FERRUM_TESTS_SUBMERGED_COATING_CASES_H

#include "dungeoncraft/coating_layout.h"

typedef enum {
    COATING_BULK_DEBIT,
    COATING_SINGLE_UNIT,
    COATING_TERMINAL_PAIR,
} coating_case_t;

static void submerged_rigid_coating_case(coating_case_t mode, unsigned hz) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    const uint32_t at=20u*64u+20u;
    chunk->cells[at].material=DC_MATERIAL_WOOD;
    const uint32_t neighbors[]={at-64u,at-1u,at+1u,at+64u};
    for(uint32_t i=0u;i<4u;i++)
        chunk->cells[neighbors[i]]=(dc_cell_t){.material=DC_MATERIAL_WATER,.fluid_mass=DC_FLUID_FULL};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    float initial_temperature=mode==COATING_TERMINAL_PAIR?293.15f:800.f;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322u,4u*65536u,
        initial_temperature,
        600100u,&wood,err,sizeof(err)));
    const uint32_t film_units[]={32768u,4u,1u};
    wood.coating_oil_fuel_fp=film_units[mode];
    wood.coating_jelly_fuel_fp=wood.coating_oil_fuel_fp;
    wood.coating_oxidizer_fp=wood.coating_oil_fuel_fp;
    if(mode!=COATING_BULK_DEBIT) wood.coating_remainders=
        FR_COATING_REACTION_REM_MASK<<FR_COATING_REACTION_REM_SHIFT;
    wood.coating_threshold=.01f;
    wood.coating_integral=.02f;wood.flags|=FR_REACTIVE_COATING_FLAMING;
    wood.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&wood,initial_temperature);
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(DC_CHUNK_CELLS,sizeof(*gas));ASSERT_TRUE(gas);
    double initial_energy=fr_reactive_state_energy(&wood);
    double initial_oxygen=0.;
    if(mode==COATING_TERMINAL_PAIR) {
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,gas,DC_CHUNK_CELLS,err,sizeof(err)));
        for(uint32_t i=0u;i<DC_CHUNK_CELLS;++i) {
            initial_energy+=gas[i].energy;
            initial_oxygen+=gas[i].species[FR_GAS_OXYGEN];
        }
    }
    unsigned steps=mode==COATING_TERMINAL_PAIR?50u*hz:1u;
    for(unsigned step=0;step<steps;++step)
        ASSERT_TRUE(fr_combustion_gpu_step(g,1.f/hz,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0u,0u,&result,&found,err,sizeof(err))&&found);
    if(mode==COATING_SINGLE_UNIT) {
        printf("Single underwater reaction: oil %u, jelly %u Q16 remain\n",
            result.state.coating_oil_fuel_fp,result.state.coating_jelly_fuel_fp);
        ASSERT_EQ(result.state.coating_oil_fuel_fp,3u);
        ASSERT_EQ(result.state.coating_jelly_fuel_fp,4u);
        ASSERT_EQ(result.state.coating_oxidizer_fp,3u);
    }
    if(mode==COATING_TERMINAL_PAIR) {
        printf("Vulkan terminal pair at %u Hz: oil %u, jelly %u, oxidizer %u Q16\n",
            hz,result.state.coating_oil_fuel_fp,result.state.coating_jelly_fuel_fp,
            result.state.coating_oxidizer_fp);
        ASSERT_EQ(result.state.coating_oil_fuel_fp,0u);
        ASSERT_EQ(result.state.coating_jelly_fuel_fp,0u);
        ASSERT_EQ(result.state.coating_oxidizer_fp,0u);
        ASSERT_EQ(result.state.flags&FR_REACTIVE_COATING_FLAMING,0u);
    }
    ASSERT_TRUE(result.state.coating_oil_fuel_fp<wood.coating_oil_fuel_fp);
    ASSERT_TRUE(result.state.coating_oxidizer_fp<wood.coating_oxidizer_fp);
    ASSERT_EQ(result.state.fuel_mass_fp,wood.fuel_mass_fp);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,gas,DC_CHUNK_CELLS,err,sizeof(err))&&
        dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    if(mode==COATING_TERMINAL_PAIR) {
        double final_energy=fr_reactive_state_energy(&result.state);
        double final_oxygen=0.;
        for(uint32_t i=0u;i<DC_CHUNK_CELLS;++i) {
            final_energy+=gas[i].energy;
            final_oxygen+=gas[i].species[FR_GAS_OXYGEN];
        }
        double heat=(fr_reactive_profile(DC_MATERIAL_OIL)->primary_heat+
            fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY)->primary_heat)/DC_FLUID_FULL;
        ASSERT_TRUE(fabs(final_oxygen-initial_oxygen)<1e-8);
        ASSERT_TRUE(fabs(final_energy-initial_energy-heat)<2e-5);
    }
    double products=0.;
    for(uint32_t i=0u;i<4u;i++) {
        products+=gas[neighbors[i]].species[FR_GAS_EXHAUST]+gas[neighbors[i]].species[FR_GAS_SOOT];
        ASSERT_EQ(chunk->cells[neighbors[i]].fluid_mass,DC_FLUID_FULL);
    }
    ASSERT_TRUE(products>0.);
    const uint32_t spent = wood.coating_oil_fuel_fp - result.state.coating_oil_fuel_fp +
                           wood.coating_jelly_fuel_fp - result.state.coating_jelly_fuel_fp +
                           wood.coating_oxidizer_fp - result.state.coating_oxidizer_fp;
    const uint32_t ash = result.state.inert_mass_fp - wood.inert_mass_fp;
    ASSERT_TRUE(fabs(products * DC_FLUID_FULL - (spent - ash)) < .001);
    ASSERT_EQ(result.state.bound_water_fp, wood.bound_water_fp);
    fr_gas_checkpoint_chunk_t *checkpoint = malloc(sizeof(*checkpoint));
    ASSERT_TRUE(checkpoint && fr_gas_gpu_export_checkpoint(g, 0, checkpoint, err, sizeof(err)) &&
                fr_gas_gpu_import_checkpoint(g, 0, checkpoint, err, sizeof(err)) &&
                fr_gas_gpu_set_pressure_policy(g, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
                fr_gas_gpu_step(g, .02f, err, sizeof(err)) &&
                fr_gas_gpu_read_chunk(g, 0, gas, DC_CHUNK_CELLS, err, sizeof(err)) &&
                dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    double transported = 0.;
    uint64_t water = 0u;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        transported += gas[i].species[FR_GAS_EXHAUST] + gas[i].species[FR_GAS_SOOT];
        water += chunk->cells[i].fluid_mass;
    }
    ASSERT_TRUE(fabs(transported - products) < 1e-6);
    ASSERT_EQ(water, UINT64_C(4) * DC_FLUID_FULL);
    free(checkpoint);
    free(gas);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_submerged_rigid_coating_burns_from_bound_oxidizer(void) {
    submerged_rigid_coating_case(COATING_BULK_DEBIT,10u);
}
static void test_submerged_rigid_coating_splits_one_whole_reaction_unit(void) {
    submerged_rigid_coating_case(COATING_SINGLE_UNIT,60u);
}
static void test_submerged_rigid_coating_exhausts_at_60_hz(void) {
    submerged_rigid_coating_case(COATING_TERMINAL_PAIR,60u);
}
static void test_submerged_rigid_coating_exhausts_at_120_hz(void) {
    submerged_rigid_coating_case(COATING_TERMINAL_PAIR,120u);
}

#endif
