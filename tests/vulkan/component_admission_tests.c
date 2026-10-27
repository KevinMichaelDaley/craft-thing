#define main component_admission_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include "ferrum/fire/thermal_gpu.h"
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
    chunk->cells[651]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t hot,cold;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,900,&hot,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,300,901,&cold,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,650,&hot,err,sizeof(err))&&fr_gpu_reactive_seed_cell(g,0,651,&cold,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    fr_reactive_record_t a,b;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&a,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,651,&b,&found,err,sizeof(err))&&found);
    float ta=0,tb=0;ASSERT_TRUE(fr_reactive_state_temperature(&a.state,&ta,err,sizeof(err))&&fr_reactive_state_temperature(&b.state,&tb,err,sizeof(err)));
    ASSERT_TRUE(ta<700&&ta>300&&tb>300&&tb<700);
    ASSERT_TRUE(fabs((double)a.state.thermal_energy+(double)b.state.thermal_energy-(double)hot.thermal_energy-(double)cold.thermal_energy)<.01);
    free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_cold_rigid_admission_retains_pose_and_reactive_ownership);RUN(test_rigid_terrain_contact_exchanges_a_paired_heat_budget);printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
