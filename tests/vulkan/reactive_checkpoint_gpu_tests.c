#define main reactive_checkpoint_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main

static void test_structure_checkpoint_retains_extracted_body_state(void) {
    char err[256]={0};const char *path="build/reactive_body_checkpoint.bin";dc_gpu_t *g=NULL;
    dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(uint32_t y=10;y<14;y++) for(uint32_t x=10;x<14;x++) c->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,900,700,91,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.8f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,11*64+11,&state,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g)&&dc_gpu_rigid_step(g,err,sizeof(err)));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&before,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_save_structure(g,path,err,sizeof(err)));dc_gpu_destroy(g);g=NULL;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&fr_gpu_load_structure(g,path,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,1,1,&after,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&before,&after,sizeof(before))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_pending_negative_checkpoint_chunk_restores_its_exact_channel(void) {
    char err[256]={0};const char *path="build/reactive_chunk_checkpoint.bin";dc_gpu_t *g=NULL;
    dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);c->coord=(dc_chunk_coord_t){-5,-7};c->cells[0].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err)));
    fr_reactive_state_t state;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,900,700,91,&state,err,sizeof(err)));
    state.coating_oil_fuel_fp=500;state.ignition_threshold=.8f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,0,&state,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g,path,err,sizeof(err)));dc_gpu_destroy(g);g=NULL;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&fr_gpu_load_structure(g,path,err,sizeof(err)));
    bool restored=false;ASSERT_TRUE(fr_gpu_upload_structure_chunk(g,1,c,&restored,err,sizeof(err))&&restored);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,1,0,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&record.state,&state,sizeof(state))==0);
    fr_reactive_owner_t owner=fr_reactive_cell_owner(c->coord,0);ASSERT_TRUE(memcmp(&record.owner,&owner,sizeof(owner))==0);
    fr_reactive_chunk_t saved={0};ASSERT_TRUE(fr_gpu_reactive_export_chunk(g,1,c,&saved,err,sizeof(err)));ASSERT_EQ(saved.next_id,92u);
    fr_reactive_chunk_release(&saved);ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_structure_checkpoint_retains_extracted_body_state);
    RUN(test_pending_negative_checkpoint_chunk_restores_its_exact_channel);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
