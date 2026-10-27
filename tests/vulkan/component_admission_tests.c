#define main component_admission_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
static void test_cold_rigid_admission_retains_pose_and_reactive_ownership(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t y=10;y<12;y++) for(uint32_t x=10;x<13;x++) chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    for(uint32_t y=10;y<12;y++) for(uint32_t x=20;x<23;x++) chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t original;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,500,&original,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,10*64+10,&original,err,sizeof(err)));
    uint32_t parcel=501;
    for(uint32_t y=0;y<2;y++) for(uint32_t x=0;x<3;x++) if(x||y) {
        fr_reactive_state_t cold;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,parcel++,&cold,err,sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,(10+y)*64+10+x,&cold,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    dc_gpu_body_t body;ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_EQ(body.x_fp,10*65536);ASSERT_EQ(body.y_fp,10*65536);
    ASSERT_EQ(body.vx_fp,0);ASSERT_EQ(body.vy_fp,0);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.owner.kind,FR_REACTIVE_OWNER_BODY);ASSERT_EQ(memcmp(&record.state,&original,sizeof(original)),0);
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_thermal_inspection_t inspection;
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,10,10,&inspection,err,sizeof(err)));
    ASSERT_TRUE(inspection.occupied&&inspection.material==DC_MATERIAL_WOOD&&fabsf(inspection.temperature_K-700)<.01f);
    ASSERT_TRUE(dc_gpu_set_body_motion(g,body.id,.6f,0,0,err,sizeof(err))&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,10,10,&inspection,err,sizeof(err)));
    printf("Rotated rigid heat sample %.3fK\n",inspection.temperature_K);
    ASSERT_TRUE(inspection.occupied&&inspection.material==DC_MATERIAL_WOOD&&inspection.temperature_K>300&&inspection.temperature_K<700);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,body.id,0,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&record.state,&original,sizeof(original)),0);
    ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,body.id,0,0,&record,&found,err,sizeof(err))&&found);
    float hot_K=0;ASSERT_TRUE(fr_reactive_state_temperature(&record.state,&hot_K,err,sizeof(err)));
    ASSERT_TRUE(hot_K<700&&hot_K>300);
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,body.id,1,0,&record,&found,err,sizeof(err))&&found);
    float cold_K=0;ASSERT_TRUE(fr_reactive_state_temperature(&record.state,&cold_K,err,sizeof(err)));
    ASSERT_TRUE(cold_K>300&&cold_K<700);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));ASSERT_EQ(chunk->cells[10*64+10].material,DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000002u,&body,err,sizeof(err)));ASSERT_EQ(body.x_fp,20*65536);
    free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_rigid_terrain_contact_exchanges_a_paired_heat_budget(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[650].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t hot,cold;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,900,&hot,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,300,901,&cold,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,650,&hot,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    chunk->cells[650].material=DC_MATERIAL_AIR;
    chunk->cells[651]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,651,&cold,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    fr_reactive_record_t a,b;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&a,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,651,&b,&found,err,sizeof(err))&&found);
    float ta=0,tb=0;ASSERT_TRUE(fr_reactive_state_temperature(&a.state,&ta,err,sizeof(err))&&fr_reactive_state_temperature(&b.state,&tb,err,sizeof(err)));
    ASSERT_TRUE(ta<700&&ta>300&&tb>300&&tb<700);
    ASSERT_TRUE(fabs((double)a.state.thermal_energy+(double)b.state.thermal_energy-(double)hot.thermal_energy-(double)cold.thermal_energy)<.01);
    free(chunk);dc_gpu_destroy(g);PASS();
}
static void rigid_gas_case(bool burning) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[650].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t hot;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,950,&hot,err,sizeof(err)));
    if(burning) hot.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,650,&hot,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    if(burning) ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,.6f,0,0,err,sizeof(err))&&
        dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_combustion_gpu_step(g,1.f/60.f,err,sizeof(err)));
    fr_reactive_record_t a;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&a,&found,err,sizeof(err))&&found);
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&a.state,&temperature,err,sizeof(err)));
    ASSERT_TRUE(temperature>293.15f);if(!burning) ASSERT_TRUE(temperature<700);
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas));ASSERT_TRUE(gas&&fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    fr_gas_thermo_t air;ASSERT_TRUE(fr_gas_thermodynamics(&model,gas+651,1,&air,err,sizeof(err)));
    if(!burning) ASSERT_TRUE(air.temperature_K>293.15);
    if(burning) {
        ASSERT_TRUE(a.state.fuel_mass_fp<hot.fuel_mass_fp);
        dc_gpu_body_motion_t body;ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&body,err,sizeof(err)));
        ASSERT_TRUE(fabs((double)body.mass-(double)fr_reactive_state_mass_fp(&a.state)/65536.)<1e-7);
    }
    free(gas);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_rigid_ember_transfers_real_heat_into_surrounding_gas(void) {rigid_gas_case(false);}
static void test_rigid_combustion_consumes_authoritative_body_fuel(void) {rigid_gas_case(true);}
static bool radiant_case(bool blocked,bool diagonal,double *received,char *err,uint32_t cap) {
    dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));if(!chunk) return false;
    uint32_t target=diagonal?13u*64u+13u:653u;
    chunk->cells[650].material=DC_MATERIAL_WOOD;chunk->cells[target].material=DC_MATERIAL_WOOD;
    fr_reactive_state_t hot,cold;bool okay=context(&g,err,cap)&&dc_gpu_upload_chunk(g,0,chunk,err,cap)&&dc_gpu_set_page(g,0,0,0,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,1400,1100,&hot,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,1101,&cold,err,cap);
    if(okay) okay=fr_gpu_reactive_seed_cell(g,0,650,&hot,err,cap)&&fr_gpu_reactive_seed_cell(g,0,target,&cold,err,cap)&&dc_gpu_admit_authored_rigid_cells(g,err,cap);
    if(okay&&blocked) {
        chunk->cells[650].material=chunk->cells[target].material=DC_MATERIAL_AIR;
        chunk->cells[651]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
        fr_reactive_state_t stone;okay=fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,300,1102,&stone,err,cap)&&
            dc_gpu_upload_chunk(g,0,chunk,err,cap)&&fr_gpu_reactive_seed_cell(g,0,651,&stone,err,cap)&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,cap);
    }
    if(okay) okay=fr_thermal_gpu_enable(g,err,cap)&&fr_thermal_gpu_mark_bonfire_world(g);
    for(uint32_t i=0;i<20&&okay;i++) okay=fr_thermal_gpu_step(g,.05f,err,cap);
    fr_reactive_record_t a,b;bool found=false;
    if(okay) okay=fr_gpu_reactive_read_body(g,0x80000001u,0,0,&a,&found,err,cap)&&found&&
        fr_gpu_reactive_read_body(g,0x80000002u,0,0,&b,&found,err,cap)&&found;
    if(okay) {
        *received=fr_reactive_state_energy(&b.state)-fr_reactive_state_energy(&cold);
        if(!blocked) okay=fabs(fr_reactive_state_energy(&a.state)+fr_reactive_state_energy(&b.state)-fr_reactive_state_energy(&hot)-fr_reactive_state_energy(&cold))<.01;
    }
    free(chunk);dc_gpu_destroy(g);return okay;
}
static void test_rigid_radiance_crosses_air_is_shadowed_and_transfers_finite_heat(void) {
    char err[256]={0};double open=0,blocked=0,diagonal=0;
    ASSERT_TRUE(radiant_case(false,false,&open,err,sizeof(err))&&radiant_case(true,false,&blocked,err,sizeof(err))&&
        radiant_case(false,true,&diagonal,err,sizeof(err)));
    printf("Radiant received open%.9g blocked%.9g diagonal%.9g\n",open,blocked,diagonal);
    ASSERT_TRUE(open>.1&&blocked<open*.1&&diagonal>.1);PASS();
}
int main(int argc,char **argv) {
    if(argc==2&&strcmp(argv[1],"--burn")==0) RUN(test_rigid_combustion_consumes_authoritative_body_fuel);
    else if(argc==2&&strcmp(argv[1],"--radiation")==0) RUN(test_rigid_radiance_crosses_air_is_shadowed_and_transfers_finite_heat);
    else {RUN(test_cold_rigid_admission_retains_pose_and_reactive_ownership);RUN(test_rigid_terrain_contact_exchanges_a_paired_heat_budget);RUN(test_rigid_ember_transfers_real_heat_into_surrounding_gas);RUN(test_rigid_combustion_consumes_authoritative_body_fuel);RUN(test_rigid_radiance_crosses_air_is_shadowed_and_transfers_finite_heat);}
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
