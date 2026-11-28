#define main pixel_component_fixture_main
#include "pixel_component_tests.c"
#undef main

static void check_cached_contact_ownership(dc_gpu_t *gpu,dc_chunk_t *chunk,unsigned row) {
    char err[256]={0};
    memset(chunk,0,sizeof(*chunk));chunk->coord.x=1;
    for(unsigned x=0;x<12;++x) chunk->cells[row*64u+x].material=DC_MATERIAL_STONE;
    for(unsigned replaced=0;replaced<2;++replaced) {
        if(replaced) chunk->coord.x=17;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,1,chunk,err,sizeof(err)));
        gpu->body_refresh_pending=true;
        ASSERT_TRUE(dc_gpu_set_body_origin(gpu,(dc_chunk_coord_t){0,0},err,sizeof(err)));
        dc_gpu_broadphase_stats_t candidates;
        dc_gpu_broadphase_pair_t pairs[32];
        ASSERT_TRUE(dc_gpu_read_broadphase(gpu,&candidates,pairs,32,err,sizeof(err)));
        unsigned matching=0;
        for(unsigned i=0;i<candidates.count;++i) if(pairs[i].chunk.x==1 && pairs[i].chunk.y==0) {
            ASSERT_EQ(pairs[i].kind,replaced ? DC_GPU_PAIR_BOUNDARY : DC_GPU_PAIR_TERRAIN);
            matching++;
        }
        ASSERT_EQ(matching,1u);
        dc_gpu_contact_stats_t stats;
        dc_gpu_contact_t contacts[512];
        ASSERT_TRUE(dc_gpu_read_contacts(gpu,&stats,contacts,512,err,sizeof(err)));
        ASSERT_EQ(stats.overflow,0u);ASSERT_TRUE(stats.count>0);
        for(unsigned i=0;i<stats.count;++i) {
            ASSERT_EQ(contacts[i].feature_chunk.x,1);
            ASSERT_EQ(contacts[i].kind,replaced ? DC_GPU_CONTACT_BOUNDARY : DC_GPU_CONTACT_TERRAIN);
            if(!replaced) ASSERT_EQ(contacts[i].feature_b/64u,row);
        }
    }
}

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
    if(world_x==60) check_cached_contact_ownership(gpu,chunk,(unsigned)(body.body.y_fp>>16)+8u);
    dc_gpu_destroy(gpu);free(chunk);
    ASSERT_EQ(pixels,128u);ASSERT_TRUE(fabsf(mass-128.f*2.7f)<.002f);
    ASSERT_EQ(velocity,4*16384);PASS();
}
static void test_onscreen_known_stone_advances(void) {cached_stone(20);}
static void test_partly_offscreen_known_stone_advances(void) {cached_stone(60);}
static void test_wholly_offscreen_known_stone_across_cached_seam_advances(void) {cached_stone(124);}
int main(int argc,char **argv) {
    if(argc==2 && strcmp(argv[1],"--seam")==0) {
        RUN(test_partly_offscreen_known_stone_advances);
        return g_fail!=0;
    }
    RUN(test_onscreen_known_stone_advances);
    RUN(test_partly_offscreen_known_stone_advances);
    RUN(test_wholly_offscreen_known_stone_across_cached_seam_advances);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
