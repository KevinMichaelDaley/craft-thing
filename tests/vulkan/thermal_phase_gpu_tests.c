#define main reactive_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "dungeoncraft/coating_layout.h"
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
    double exact_oil=(double)gained+
        (double)((record.state.coating_remainders>>FR_COATING_OIL_REM_SHIFT)&
            FR_COATING_REM_MASK)/FR_COATING_MASS_DEN;
    ASSERT_TRUE(fabs(exact_oil-(double)lost*FR_COATING_OIL_MASS_NUM/
        FR_COATING_MASS_DEN)<1e-6);
    ASSERT_TRUE(after->oil_excess[oil_at]<phase->oil_excess[oil_at]);
    ASSERT_TRUE(fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_gpu_reactive_read_body(g,0x80000001u,0u,0u,&record,&found,err,sizeof(err))&&found&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint32_t total_lost=phase->oil_fp[oil_at]-after->oil_fp[oil_at];
    ASSERT_TRUE(total_lost>lost);
    double total_mass=(double)record.state.coating_oil_fuel_fp+
        (double)((record.state.coating_remainders>>FR_COATING_OIL_REM_SHIFT)&
            FR_COATING_REM_MASK)/FR_COATING_MASS_DEN;
    ASSERT_TRUE(fabs(total_mass-(double)total_lost*FR_COATING_OIL_MASS_NUM/
        FR_COATING_MASS_DEN)<1e-6);
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
static void test_rigid_contact_preserves_jelly_fuel_and_oxidizer(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&phase&&after);
    for(uint32_t y=20u;y<22u;y++) for(uint32_t x=20u;x<23u;x++)
        chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
    uint32_t at=20u*64u+19u;phase->slime_fp[at]=30000u;
    phase->slime_excess[at]=3.f;phase->oil_fp[at]=10000u;
    phase->oil_excess[at]=2.f;chunk->cells[at].fluid_mass=10000u;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322u,0u,300.f,
        502u,&wood,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,20u*64u+20u,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,phase,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0u,0u,&record,&found,err,sizeof(err))&&found&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint32_t lost=phase->slime_fp[at]-after->slime_fp[at];
    uint32_t fuel=record.state.coating_jelly_fuel_fp;
    uint32_t oxidizer=record.state.coating_oxidizer_fp;
    const fr_reactive_profile_t *jelly=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    ASSERT_TRUE(lost>0u&&fuel>0u&&oxidizer>0u&&jelly);
    ASSERT_TRUE(fabs((double)fuel+oxidizer-(double)lost*jelly->density)<2.);
    ASSERT_TRUE(fabs((double)fuel-(double)(fuel+oxidizer)*jelly->fuel_fraction)<2.);
    double exact_jelly=(double)fuel+oxidizer+
        (double)((record.state.coating_remainders>>FR_COATING_JELLY_REM_SHIFT)&
            FR_COATING_REM_MASK)/FR_COATING_MASS_DEN;
    ASSERT_TRUE(fabs(exact_jelly-(double)lost*FR_COATING_JELLY_MASS_NUM/
        FR_COATING_MASS_DEN)<1e-6);
    ASSERT_TRUE(after->slime_excess[at]<phase->slime_excess[at]);
    ASSERT_TRUE(record.state.coating_oil_fuel_fp>0u&&after->oil_fp[at]<phase->oil_fp[at]);
    dc_chunk_t *water_after=calloc(1,sizeof(*water_after));ASSERT_TRUE(water_after);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,water_after,err,sizeof(err))&&
        water_after->cells[at].fluid_mass>0u);
    free(water_after);
    free(after);free(phase);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_stone_surface_wets_from_neighbor_oil_without_burning_stone(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&phase&&after);
    uint32_t stone_at=20u*64u+20u,oil_at=20u*64u+19u;
    chunk->cells[stone_at].material=DC_MATERIAL_STONE;
    phase->oil_fp[oil_at]=30000u;phase->oil_excess[oil_at]=2.f;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t stone;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,300.f,
        503u,&stone,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,stone_at,&stone,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,phase,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,stone_at,&record,&found,err,sizeof(err))&&found&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint32_t lost=phase->oil_fp[oil_at]-after->oil_fp[oil_at];
    double attached=record.state.coating_oil_fuel_fp+
        (double)((record.state.coating_remainders>>FR_COATING_OIL_REM_SHIFT)&
            FR_COATING_REM_MASK)/FR_COATING_MASS_DEN;
    ASSERT_TRUE(lost>0u&&fabs(attached-(double)lost*FR_COATING_OIL_MASS_NUM/
        FR_COATING_MASS_DEN)<1e-6);
    ASSERT_EQ(record.state.fuel_mass_fp,0u);
    free(after);free(phase);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_hot_terrain_oil_drips_into_finite_bulk_while_cold_film_holds(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&phase&&after);
    uint32_t hot_at=20u*64u+20u,cold_at=20u*64u+30u;
    chunk->cells[hot_at].material=DC_MATERIAL_STONE;
    chunk->cells[cold_at].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    for(uint32_t control=0u;control<2u;control++) {
        uint32_t at=control==0u?hot_at:cold_at;
        float temperature=control==0u?500.f:300.f;
        fr_reactive_state_t state;
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,300.f,
            601u+control,&state,err,sizeof(err)));
        state.coating_oil_fuel_fp=44000u;
        ASSERT_TRUE(fr_reactive_state_add_heat(&state,
            fr_reactive_state_energy_at_temperature(&state,temperature)-
            fr_reactive_state_energy(&state),err,sizeof(err))&&
            fr_gpu_reactive_seed_cell(g,0,at,&state,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(g,0,chunk,phase,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    fr_reactive_record_t hot,cold;bool hot_found=false,cold_found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,hot_at,&hot,&hot_found,err,sizeof(err))&&
        fr_gpu_reactive_read_cell(g,0,cold_at,&cold,&cold_found,err,sizeof(err))&&
        hot_found&&cold_found);
    uint32_t released=after->oil_fp[hot_at+64u];
    ASSERT_TRUE(released>0u&&hot.state.coating_oil_fuel_fp<44000u);
    ASSERT_EQ(44000u-hot.state.coating_oil_fuel_fp,
        released*FR_COATING_OIL_MASS_NUM/FR_COATING_MASS_DEN);
    ASSERT_EQ(cold.state.coating_oil_fuel_fp,44000u);
    ASSERT_EQ(after->oil_fp[cold_at+64u],0u);
    free(after);free(phase);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_moving_hot_body_piece_drips_from_its_rest_attachment(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *phase=calloc(1,sizeof(*phase));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&phase&&after);
    for(uint32_t y=20u;y<22u;y++) for(uint32_t x=20u;x<23u;x++)
        chunk->cells[y*64u+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322u,0u,300.f,
        611u,&wood,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,21u*64u+20u,&wood,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    fr_reactive_record_t before;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0u,1u,&before,&found,
        err,sizeof(err))&&found);
    before.state.coating_oil_fuel_fp=44000u;
    ASSERT_TRUE(fr_reactive_state_add_heat(&before.state,
        fr_reactive_state_energy_at_temperature(&before.state,500.f)-
        fr_reactive_state_energy(&before.state),err,sizeof(err))&&
        fr_gpu_reactive_seed_body(g,0x80000001u,0u,1u,&before.state,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,phase,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        dc_gpu_set_body_motion(g,0x80000001u,.2f,0.f,0.f,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_gpu_reactive_read_body(g,0x80000001u,0u,1u,&before,&found,
            err,sizeof(err))&&found&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint32_t released=0u;
    for(uint32_t i=0u;i<DC_CHUNK_CELLS;i++) released+=after->oil_fp[i];
    ASSERT_TRUE(released>0u&&before.state.coating_oil_fuel_fp<44000u);
    ASSERT_EQ(44000u-before.state.coating_oil_fuel_fp,
        released*FR_COATING_OIL_MASS_NUM/FR_COATING_MASS_DEN);
    ASSERT_EQ(before.state.origin_id_low,wood.origin_id_low);
    free(after);free(phase);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_detached_film_activates_bulk_transport(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *empty=calloc(1,sizeof(*empty));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&empty&&after);
    uint32_t stone_at=20u*64u+20u,drip_at=stone_at+64u;
    chunk->cells[stone_at].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t stone;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,300.f,
        621u,&stone,err,sizeof(err)));
    stone.coating_oil_fuel_fp=44000u;
    ASSERT_TRUE(fr_reactive_state_add_heat(&stone,
        fr_reactive_state_energy_at_temperature(&stone,500.f)-
        fr_reactive_state_energy(&stone),err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,stone_at,&stone,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,empty,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    ASSERT_TRUE(after->oil_fp[drip_at]>0u);
    uint32_t released=after->oil_fp[drip_at];
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    uint64_t total=0u;for(uint32_t i=0u;i<DC_CHUNK_CELLS;i++) total+=after->oil_fp[i];
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,stone_at,&record,&found,err,sizeof(err))&&found);
    uint32_t film_rem=(record.state.coating_remainders>>FR_COATING_OIL_REM_SHIFT)&
        FR_COATING_REM_MASK;
    ASSERT_EQ(total+(record.state.coating_oil_fuel_fp*FR_COATING_MASS_DEN+film_rem)/
        FR_COATING_OIL_MASS_NUM,40000u);
    ASSERT_TRUE(after->oil_fp[drip_at]<released);
    free(after);free(empty);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_spent_jelly_oxidizer_does_not_reform_fresh_bulk_slime(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_bulk_phase_chunk_t *empty=calloc(1,sizeof(*empty));
    fr_bulk_phase_chunk_t *after=calloc(1,sizeof(*after));
    ASSERT_TRUE(chunk&&empty&&after);
    uint32_t stone_at=20u*64u+20u;
    chunk->cells[stone_at].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&
        dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t stone;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,300.f,
        631u,&stone,err,sizeof(err)));
    stone.coating_oxidizer_fp=52000u;
    ASSERT_TRUE(fr_reactive_state_add_heat(&stone,
        fr_reactive_state_energy_at_temperature(&stone,500.f)-
        fr_reactive_state_energy(&stone),err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,stone_at,&stone,err,sizeof(err))&&
        fr_bulk_phase_gpu_import_chunk(g,0,chunk,empty,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_step(g,.5f,err,sizeof(err))&&
        fr_bulk_phase_gpu_export_chunk(g,0,after,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,stone_at,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(after->slime_fp[stone_at+64u],0u);
    ASSERT_EQ(record.state.coating_jelly_fuel_fp,0u);
    ASSERT_EQ(record.state.coating_oxidizer_fp,52000u);
    ASSERT_TRUE(fr_reactive_state_valid(&record.state,err,sizeof(err)));
    free(after);free(empty);free(chunk);dc_gpu_destroy(g);PASS();
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
    RUN(test_rigid_contact_preserves_jelly_fuel_and_oxidizer);
    RUN(test_stone_surface_wets_from_neighbor_oil_without_burning_stone);
    RUN(test_hot_terrain_oil_drips_into_finite_bulk_while_cold_film_holds);
    RUN(test_moving_hot_body_piece_drips_from_its_rest_attachment);
    RUN(test_detached_film_activates_bulk_transport);
    RUN(test_spent_jelly_oxidizer_does_not_reform_fresh_bulk_slime);
    RUN(test_saturated_neighbor_air_suppresses_drying);
    RUN(test_equal_simulated_time_dries_within_one_water_quantum);
    RUN(test_drying_reaches_gas_across_a_chunk_seam);
    RUN(test_maintained_wet_wood_does_not_ignite_beside_dry_control);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
