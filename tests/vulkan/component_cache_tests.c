#define main extraction_fixture_main
#include "component_extraction_tests.c"
#undef main
#include "../../src/vulkan/runtime_body_internal.h"

static void test_unchanged_membership_reuses_chunk_components_and_edits_invalidate(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned i=0;i<4096;i++) chunk->cells[i].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[44],1u);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[44],0u);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[44],1u);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    unsigned count=0;for(unsigned i=0;i<4096;i++) count+=chunk->cells[i].material==DC_MATERIAL_STONE;
    ASSERT_EQ(count,4095u);ASSERT_EQ(chunk->cells[20*64+20].material,DC_MATERIAL_AIR);
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_cached_component_detaches_immediately_when_its_bridge_is_cut(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned x=0;x<32;x++) chunk->cells[20*64+x].material=DC_MATERIAL_STONE;
    for(unsigned y=18;y<25;y++) for(unsigned x=30;x<38;x++) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,10,20,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[44],0u);
    ASSERT_TRUE(dc_gpu_paint_material(g,16,20,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    uint32_t count=0;dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,NULL,0,&count,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000001u,&motion,err,sizeof(err)));
    ASSERT_EQ(count,69u);ASSERT_TRUE(fabsf(motion.mass-69*2.7f)<.001f);
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_reductions_keep_adjacent_subgroup_components_and_materials_distinct(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned component=0;component<8;component++) {
        unsigned x=3+(component%4)*12,y=3+(component/4)*12;
        for(unsigned dy=0;dy<4;dy++) for(unsigned dx=0;dx<4;dx++)
            chunk->cells[(y+dy)*64+x+dx].material=component%2?DC_MATERIAL_WOOD:DC_MATERIAL_STONE;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    for(unsigned serial=1;serial<=8;serial++) {
        uint32_t count=0;dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000000u+serial,NULL,0,&count,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,0x80000000u+serial,&motion,err,sizeof(err)));
        ASSERT_EQ(count,16u);ASSERT_TRUE(fabsf(motion.mass-16*(serial%2?2.7f:.6f))<.001f);
    }
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_boundary_ground_skips_candidate_bounds_work(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=32;y<64;y++) for(unsigned x=0;x<64;x++)
        chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[49],1u);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    unsigned count=0;for(unsigned i=0;i<4096;i++) count+=chunk->cells[i].material==DC_MATERIAL_STONE;
    ASSERT_EQ(count,2048u);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,40,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_EQ(((uint32_t *)g->runtime_bodies->work.mapped)[49],1u);
    dc_gpu_destroy(g);free(chunk);PASS();
}
int main(void) {
    RUN(test_boundary_ground_skips_candidate_bounds_work);
    RUN(test_unchanged_membership_reuses_chunk_components_and_edits_invalidate);
    RUN(test_cached_component_detaches_immediately_when_its_bridge_is_cut);
    RUN(test_reductions_keep_adjacent_subgroup_components_and_materials_distinct);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
