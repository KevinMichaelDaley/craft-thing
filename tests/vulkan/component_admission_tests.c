#define main component_admission_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
static void test_cold_rigid_admission_retains_pose_and_reactive_ownership(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t y=10;y<12;y++) for(uint32_t x=10;x<13;x++) chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    for(uint32_t y=10;y<12;y++) for(uint32_t x=20;x<23;x++) chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t original;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,500,&original,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,10*64+10,&original,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err)));
    dc_gpu_body_t body;ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_EQ(body.x_fp,10*65536);ASSERT_EQ(body.y_fp,10*65536);
    ASSERT_EQ(body.vx_fp,0);ASSERT_EQ(body.vy_fp,0);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_EQ(record.owner.kind,FR_REACTIVE_OWNER_BODY);ASSERT_EQ(memcmp(&record.state,&original,sizeof(original)),0);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));ASSERT_EQ(chunk->cells[10*64+10].material,DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000002u,&body,err,sizeof(err)));ASSERT_EQ(body.x_fp,20*65536);
    free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_cold_rigid_admission_retains_pose_and_reactive_ownership);printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
