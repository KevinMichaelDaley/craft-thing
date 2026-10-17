#define main pixel_component_fixture_main
#include "pixel_component_tests.c"
#undef main

static void cached_stone(unsigned world_x) {
    char err[256]={0};dc_gpu_t *gpu=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned slot=0;slot<3;slot++) {
        memset(chunk,0,sizeof(*chunk));chunk->coord.x=slot;
        for(unsigned y=20;y<28;y++) for(unsigned x=0;x<64;x++)
            if(slot*64+x>=world_x&&slot*64+x<world_x+16)
                chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,slot,chunk,err,sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_set_page(gpu,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    for(unsigned tick=0;tick<4;tick++) ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,body.body.id,&motion,err,sizeof(err)));
    unsigned pixels=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu,body.body.id,NULL,0,&pixels,err,sizeof(err)));
    float mass=motion.mass;int velocity=body.body.vy_fp;
    printf("Known cached stone x%u: pixels%u mass%.6f vy%d\n",world_x,pixels,mass,velocity);
    dc_gpu_destroy(gpu);free(chunk);
    ASSERT_EQ(pixels,128u);ASSERT_TRUE(fabsf(mass-128.f*2.7f)<.002f);
    ASSERT_TRUE(velocity>0);PASS();
}
static void test_onscreen_known_stone_advances(void) {cached_stone(20);}
static void test_partly_offscreen_known_stone_advances(void) {cached_stone(60);}
static void test_wholly_offscreen_known_stone_across_cached_seam_advances(void) {cached_stone(124);}
int main(void) {
    RUN(test_onscreen_known_stone_advances);
    RUN(test_partly_offscreen_known_stone_advances);
    RUN(test_wholly_offscreen_known_stone_across_cached_seam_advances);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
