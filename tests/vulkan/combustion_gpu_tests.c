#define main combustion_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"

static void terrain_combustion(bool practical,bool char_stage) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,100,&wood,err,sizeof(err)));
    if(char_stage) {
        wood.char_mass_fp=wood.fuel_mass_fp;wood.fuel_mass_fp=0;
        wood.thermal_energy=(float)(fr_reactive_state_heat_capacity(&wood)*800.);
    }
    wood.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    if(practical) ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *before=calloc(4096,sizeof(*before)),*after=calloc(4096,sizeof(*after));ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp+result.state.char_mass_fp<wood.fuel_mass_fp+wood.char_mass_fp);
    ASSERT_TRUE(fr_reactive_state_valid(&result.state,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    double oxygen=0,products=0,mass_before=fr_reactive_state_mass_fp(&wood)/65536.,
        mass_after=fr_reactive_state_mass_fp(&result.state)/65536.;
    for(uint32_t i=0;i<4096;i++) {
        oxygen+=before[i].species[FR_GAS_OXYGEN]-after[i].species[FR_GAS_OXYGEN];
        products+=after[i].species[FR_GAS_EXHAUST]+after[i].species[FR_GAS_SOOT];
        for(uint32_t species=0;species<FR_GAS_SPECIES;species++) {
            mass_before+=before[i].species[species];mass_after+=after[i].species[species];
        }
    }
    ASSERT_TRUE(oxygen>0&&products>oxygen);
    ASSERT_TRUE(fabs(mass_before-mass_after)<1e-7);
    uint32_t fuel_before_short_steps=result.state.fuel_mass_fp+result.state.char_mass_fp;
    for(uint32_t step=0;step<20;step++) ASSERT_TRUE(fr_combustion_gpu_step(g,.001f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp+result.state.char_mass_fp<fuel_before_short_steps&&result.state.char_mass_fp>0);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    fr_reactive_record_t paused;ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&paused,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&result,&paused,sizeof(result)),0);
    free(before);free(after);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_displayed_terrain_combustion_debits_real_fuel_and_oxygen(void) {terrain_combustion(false,false);}
static void test_sparse_owner_combustion_uses_the_live_gas_bank(void) {terrain_combustion(true,false);}
static void test_retained_char_can_finish_burning_on_the_gpu(void) {terrain_combustion(true,true);}
static void test_spent_terrain_becomes_loose_saved_ash_without_losing_mass(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;
    chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t spent;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,2000,0,300,70000,&spent,err,sizeof(err)));
    spent.inert_mass_fp=spent.fuel_mass_fp;spent.fuel_mass_fp=0;spent.flags=FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&spent,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_enable(g,err,sizeof(err))&&fr_combustion_gpu_step(g,.01f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=true;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&!found);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_EQ(chunk->cells[at].material,DC_MATERIAL_ASH);ASSERT_EQ(chunk->cells[at].flags,0);
    ASSERT_EQ(chunk->particles[at].mass_fp,2000);ASSERT_EQ(chunk->particles[at].material,DC_MATERIAL_ASH);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,at,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.material,DC_MATERIAL_ASH);ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true)&&dc_gpu_set_tick_seconds(g,1.f/60.f));
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err))&&present(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    uint32_t particle=UINT32_MAX;
    for(uint32_t i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) if(chunk->particles[i].mass_fp) {
        ASSERT_EQ(chunk->particles[i].mass_fp,2000);ASSERT_EQ(particle,UINT32_MAX);particle=i;
    }
    ASSERT_TRUE(particle!=UINT32_MAX&&chunk->particles[particle].y_fp>20*65536+32768);
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,particle,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    ASSERT_TRUE(dc_gpu_save_bodies(g,"build/combustion_loose_ash.bin",err,sizeof(err)));
    dc_gpu_destroy(g);g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_load_bodies(g,"build/combustion_loose_ash.bin",err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_particle(g,particle,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.material,DC_MATERIAL_ASH);ASSERT_EQ(fr_reactive_state_mass_fp(&record.state),2000);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_displayed_terrain_combustion_debits_real_fuel_and_oxygen);
    RUN(test_sparse_owner_combustion_uses_the_live_gas_bank);
    RUN(test_retained_char_can_finish_burning_on_the_gpu);
    RUN(test_spent_terrain_becomes_loose_saved_ash_without_losing_mass);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
