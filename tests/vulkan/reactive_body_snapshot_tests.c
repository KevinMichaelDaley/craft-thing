#define main reactive_body_snapshot_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include "../../src/vulkan/reactive_storage_gpu.h"

static void test_inactive_body_snapshot_does_not_activate_reactive_storage(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    fr_reactive_record_t *records=NULL;uint32_t count=99;uint64_t next=99;
    ASSERT_TRUE(fr_gpu_reactive_export_bodies(g,&records,&count,&next,err,sizeof(err)));
    ASSERT_TRUE(!records);ASSERT_EQ(count,0u);ASSERT_EQ(next,1u);
    fr_gpu_reactive_stats_t stats;ASSERT_TRUE(fr_gpu_reactive_read_stats(g,&stats,err,sizeof(err)));ASSERT_EQ(stats.capacity,0u);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
static void test_gpu_body_snapshot_filters_cell_state_and_retains_exact_body_ownership(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(uint32_t y=10;y<14;y++) for(uint32_t x=10;x<14;x++) c->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t body_state,cell_state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,900,700,91,&body_state,err,sizeof(err)));
    body_state.coating_oil_fuel_fp=500;body_state.ignition_threshold=.8f;body_state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,11*64+11,&body_state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g)&&dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,30,30,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,600,92,&cell_state,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,30*64+30,&cell_state,err,sizeof(err)));
    fr_reactive_record_t expected;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&expected,&found,err,sizeof(err))&&found);
    fr_reactive_record_t *records=NULL;uint32_t count=0;uint64_t next=0;
    ASSERT_TRUE(fr_gpu_reactive_export_bodies(g,&records,&count,&next,err,sizeof(err)));
    ASSERT_EQ(count,1u);ASSERT_EQ(next,93u);ASSERT_TRUE(memcmp(records,&expected,sizeof(expected))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(records);free(c);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_inactive_body_snapshot_does_not_activate_reactive_storage);
    RUN(test_gpu_body_snapshot_filters_cell_state_and_retains_exact_body_ownership);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
