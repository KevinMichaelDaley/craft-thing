#define main reactive_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal.h"
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
    double vapor=0,gas_energy_before=0,gas_energy_after=0;
    for(uint32_t i=0;i<4096;i++) {
        vapor+=after[i].species[FR_GAS_VAPOR]-before[i].species[FR_GAS_VAPOR];
        gas_energy_before+=before[i].energy;gas_energy_after+=after[i].energy;
    }
    uint32_t dried=wood.bound_water_fp-record.state.bound_water_fp;
    ASSERT_TRUE(dried>0u);
    ASSERT_TRUE(fabs(vapor-(double)dried/65536.)<1e-6);
    ASSERT_EQ(record.state.fuel_mass_fp,wood.fuel_mass_fp);
    ASSERT_TRUE(record.state.evaporation_fraction>=0.f&&record.state.evaporation_fraction<1.f);
    double offset=0;ASSERT_TRUE(fr_thermal_vapor_reference_energy(&model,&offset,err,sizeof(err)));
    double total_before=fr_reactive_state_energy(&wood)+gas_energy_before;
    double total_after=fr_reactive_state_energy(&record.state)+gas_energy_after+vapor*offset;
    printf("GPU drying water %u Q16 energy drift %.6g kJ\n",dried,total_after-total_before);
    ASSERT_TRUE(fabs(total_after-total_before)<.001);
    ASSERT_TRUE(fr_combustion_gpu_step(g,0,err,sizeof(err)));
    fr_reactive_record_t paused;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&paused,&found,err,sizeof(err))&&found);
    ASSERT_EQ(paused.state.bound_water_fp,record.state.bound_water_fp);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    free(before);free(after);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_rotated_rigid_wood_keeps_its_drying_inventory(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t y=20;y<22;y++) for(uint32_t x=20;x<23;x++)
        chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,450,201,&wood,err,sizeof(err)));
    wood.ignition_threshold=1e9f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,20u*64u+20u,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&before,&found,err,sizeof(err))&&found);
    ASSERT_EQ(before.state.bound_water_fp,wood.bound_water_fp);
    for(uint32_t i=0;i<12;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&after,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(after.state.bound_water_fp<before.state.bound_water_fp);
    uint32_t first=after.state.bound_water_fp;
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.4f,0,0,err,sizeof(err)));
    for(uint32_t i=0;i<12;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&after,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(after.state.bound_water_fp<first);
    ASSERT_EQ(after.state.fuel_mass_fp,wood.fuel_mass_fp);
    ASSERT_EQ(after.state.origin_id_low,wood.origin_id_low);
    free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_rigid_contact_wets_from_finite_oil_volume(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&phase&&after);
    for(uint32_t y=20u;y<22u;y++) for(uint32_t x=20u;x<23u;x++)
        chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
    uint32_t oil_at=20u*64u+19u;phase->oil_fp[oil_at]=30000u;
    chunk->cells[oil_at].fluid_mass=10000u;
    phase->oil_excess[oil_at]=4.f;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322u,0u,300.f,
        501u,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,20u*64u+20u,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,phase,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_gpu_reactive_read_body(g,0x80000001u,0u,0u,&record,&found,err,sizeof(err))&&found&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint32_t gained=record.state.coating_oil_fuel_fp-wood.coating_oil_fuel_fp;
    uint32_t lost=phase->oil_fp[oil_at]-after->oil_fp[oil_at];
    printf("Rigid oil coating mass %u Q16, source volume %u Q16\n",gained,lost);
    ASSERT_TRUE(gained>0u&&lost>0u);
    ASSERT_TRUE(fabs((double)gained-(double)lost*
        fr_reactive_profile(DC_MATERIAL_OIL)->density)<2.);
    ASSERT_TRUE(after->oil_excess[oil_at]<phase->oil_excess[oil_at]);
    dc_chunk_t *water_after=calloc(1,sizeof(*water_after));ASSERT_TRUE(water_after);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,water_after,err,sizeof(err)));
    ASSERT_TRUE(water_after->cells[oil_at].fluid_mass>0u);
    ASSERT_TRUE(record.state.bound_water_fp>0u);
    uint32_t first_coating=record.state.coating_oil_fuel_fp;
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.7f,0.f,0.f,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_gpu_reactive_read_body(g,0x80000001u,0u,0u,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(record.state.coating_oil_fuel_fp>=first_coating);
    ASSERT_EQ(record.state.origin_id_low,wood.origin_id_low);
    free(water_after);
    free(after);free(phase);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_saturated_neighbor_air_suppresses_drying(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;chunk->cells[at].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,450,301,&wood,err,sizeof(err)));
    wood.ignition_threshold=1e9f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *states=calloc(4096,sizeof(*states));double *volumes=calloc(4096,sizeof(*volumes));
    ASSERT_TRUE(states&&volumes);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,states,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) {
        volumes[i]=i==at?0.:1.;if(i==at) continue;
        states[i].species[FR_GAS_VAPOR]+=.01;
        states[i].energy+=.01*model.heat_capacity_cv[FR_GAS_VAPOR]*293.15;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,states,volumes,4096,err,sizeof(err)));
    for(uint32_t i=0;i<24;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.state.bound_water_fp,wood.bound_water_fp);
    ASSERT_EQ(record.state.fuel_mass_fp,wood.fuel_mass_fp);
    free(states);free(volumes);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_equal_simulated_time_dries_within_one_water_quantum(void) {
    char err[256]={0};uint32_t remaining[2]={0};float fraction[2]={0};
    for(uint32_t trial=0;trial<2;trial++) {
        dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
        uint32_t at=20u*64u+20u;chunk->cells[at].material=DC_MATERIAL_WOOD;
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
            dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        fr_reactive_state_t wood;
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,450,401,&wood,err,sizeof(err)));
        wood.ignition_threshold=1e9f;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
        fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
            .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
        ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
            fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
            fr_combustion_gpu_enable(g,err,sizeof(err)));
        uint32_t steps=trial==0?24u:12u;float dt=trial==0?.05f:.1f;
        for(uint32_t i=0;i<steps;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,dt,err,sizeof(err)));
        fr_reactive_record_t record;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&found);
        remaining[trial]=record.state.bound_water_fp;
        fraction[trial]=record.state.evaporation_fraction;
        free(chunk);dc_gpu_destroy(g);
    }
    ASSERT_TRUE(abs((int)remaining[0]-(int)remaining[1])<=1);
    double transfer_a=10000.-remaining[0]+fraction[0];
    double transfer_b=10000.-remaining[1]+fraction[1];
    ASSERT_TRUE(fabs(transfer_a-transfer_b)<1.);
    PASS();
}
static void test_drying_reaches_gas_across_a_chunk_seam(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *left=calloc(1,sizeof(*left)),*right=calloc(1,sizeof(*right));ASSERT_TRUE(left&&right);
    right->coord.x=1;
    uint32_t at=20u*64u+63u;left->cells[at].material=DC_MATERIAL_WOOD;
    left->cells[20u*64u+62u].material=DC_MATERIAL_STONE;
    left->cells[19u*64u+63u].material=DC_MATERIAL_STONE;
    left->cells[21u*64u+63u].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,left,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,1,right,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&dc_gpu_set_page(g,1,0,1,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,10000,450,501,&wood,err,sizeof(err)));
    wood.ignition_threshold=1e9f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));ASSERT_TRUE(gas);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,1,gas,4096,err,sizeof(err)));
    double before=gas[20u*64u].species[FR_GAS_VAPOR];
    for(uint32_t i=0;i<24;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,1,gas,4096,err,sizeof(err)));
    ASSERT_TRUE(record.state.bound_water_fp<wood.bound_water_fp);
    ASSERT_TRUE(gas[20u*64u].species[FR_GAS_VAPOR]>before);
    free(gas);free(left);free(right);dc_gpu_destroy(g);PASS();
}
static void test_maintained_wet_wood_does_not_ignite_beside_dry_control(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t wet_at=20u*64u+20u,dry_at=20u*64u+24u;
    chunk->cells[wet_at].material=DC_MATERIAL_WOOD;
    chunk->cells[dry_at].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wet,dry;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,18000,650,601,&wet,err,sizeof(err))&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,650,602,&dry,err,sizeof(err)));
    wet.ignition_threshold=1.f;dry.ignition_threshold=1.f;
    wet.ignition_integral=.99f;dry.ignition_integral=.99f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,wet_at,&wet,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,dry_at,&dry,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&
        fr_combustion_gpu_enable(g,err,sizeof(err)));
    for(uint32_t i=0;i<24;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t wet_result,dry_result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,wet_at,&wet_result,&found,err,sizeof(err))&&found&&
        fr_gpu_reactive_read_cell(g,0,dry_at,&dry_result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(wet_result.state.bound_water_fp>15729u);
    ASSERT_EQ(wet_result.state.fuel_mass_fp,wet.fuel_mass_fp);
    ASSERT_TRUE((wet_result.state.flags&FR_REACTIVE_FLAMING)==0);
    ASSERT_TRUE(dry_result.state.fuel_mass_fp<dry.fuel_mass_fp);
    free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_wet_wood_dries_into_the_local_gas_without_losing_fuel);
    RUN(test_rotated_rigid_wood_keeps_its_drying_inventory);
    RUN(test_rigid_contact_wets_from_finite_oil_volume);
    RUN(test_saturated_neighbor_air_suppresses_drying);
    RUN(test_equal_simulated_time_dries_within_one_water_quantum);
    RUN(test_drying_reaches_gas_across_a_chunk_seam);
    RUN(test_maintained_wet_wood_does_not_ignite_beside_dry_control);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
