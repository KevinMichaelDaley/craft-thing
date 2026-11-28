#define main pixel_component_fixture_main
#include "pixel_component_tests.c"
#undef main

static void check_cached_contact_ownership(dc_gpu_t *gpu,dc_chunk_t *chunk,unsigned row,int floor_chunk,dc_chunk_coord_t camera) {
    char err[256]={0};
    memset(chunk,0,sizeof(*chunk));
    for(unsigned x=0;x<12;++x) chunk->cells[row*64u+x].material=DC_MATERIAL_STONE;
    const struct { unsigned slot; int chunk_x; bool has_floor; } uploads[] = {
        {(unsigned)floor_chunk,floor_chunk,true}, {(unsigned)floor_chunk,17,false}, {3,floor_chunk,true}
    };
    for(unsigned step=0;step<sizeof(uploads)/sizeof(uploads[0]);++step) {
        chunk->coord.x=uploads[step].chunk_x;
        bool has_floor=uploads[step].has_floor;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,uploads[step].slot,chunk,err,sizeof(err)));
        gpu->body_refresh_pending=true;
        ASSERT_TRUE(dc_gpu_set_body_origin(gpu,camera,err,sizeof(err)));
        dc_gpu_broadphase_stats_t candidates;
        dc_gpu_broadphase_pair_t pairs[32];
        ASSERT_TRUE(dc_gpu_read_broadphase(gpu,&candidates,pairs,32,err,sizeof(err)));
        unsigned matching=0;
        for(unsigned i=0;i<candidates.count;++i) if(pairs[i].chunk.x==floor_chunk && pairs[i].chunk.y==0) {
            ASSERT_EQ(pairs[i].kind,has_floor ? DC_GPU_PAIR_TERRAIN : DC_GPU_PAIR_BOUNDARY);
            matching++;
        }
        ASSERT_EQ(matching,1u);
        dc_gpu_contact_stats_t stats;
        dc_gpu_contact_t contacts[512];
        ASSERT_TRUE(dc_gpu_read_contacts(gpu,&stats,contacts,512,err,sizeof(err)));
        ASSERT_EQ(stats.overflow,0u);ASSERT_TRUE(stats.count>0);
        for(unsigned i=0;i<stats.count;++i) {
            ASSERT_EQ(contacts[i].feature_chunk.x,floor_chunk);
            ASSERT_EQ(contacts[i].kind,has_floor ? DC_GPU_CONTACT_TERRAIN : DC_GPU_CONTACT_BOUNDARY);
            if(has_floor) ASSERT_EQ(contacts[i].feature_b/64u,row);
        }
    }
}

static void check_cached_body_support(dc_gpu_t *gpu,dc_gpu_world_body_t stone,dc_chunk_coord_t camera) {
    char err[256]={0};
    dc_gpu_world_body_t support={.chunk={1,0},.body={.id=71,.active=1,.width=6,.height=2,
        .x_fp=63<<16,.y_fp=stone.body.y_fp+(8<<16)}};
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu,support,err,sizeof(err)));
    support.chunk.x+=INT64_C(1)<<32;
    support.body.id=72;
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu,support,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu,camera,err,sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[32];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu,&stats,pairs,32,err,sizeof(err)));
    unsigned found=0;
    for(unsigned i=0;i<stats.count;i++) if(pairs[i].kind==DC_GPU_PAIR_BODY) {
        ASSERT_EQ(pairs[i].body_a,71u);ASSERT_EQ(pairs[i].body_b,stone.body.id);
        found++;
    }
    ASSERT_EQ(found,1u);
    dc_gpu_contact_stats_t narrow;
    dc_gpu_contact_t contacts[512];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu,&narrow,contacts,512,err,sizeof(err)));
    found=0;
    for(unsigned i=0;i<narrow.count;i++) if(contacts[i].kind==DC_GPU_CONTACT_BODY) {
        ASSERT_EQ(contacts[i].body_a,71u);ASSERT_EQ(contacts[i].body_b,stone.body.id);
        found++;
    }
    ASSERT_TRUE(found>0);
}

static void cached_stone(unsigned world_x,unsigned camera_chunk) {
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
    ASSERT_TRUE(dc_gpu_set_page(gpu,0,0,camera_chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu,(dc_chunk_coord_t){camera_chunk,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    for(unsigned tick=0;tick<4;tick++) ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,body.body.id,&motion,err,sizeof(err)));
    dc_gpu_body_t solver_body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,body.body.id,&solver_body,err,sizeof(err)));
    int64_t expected_x=(body.chunk.x-camera_chunk)*(64<<16)+body.body.x_fp;
    int64_t expected_y=body.chunk.y*(64<<16)+body.body.y_fp;
    unsigned pixels=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu,body.body.id,NULL,0,&pixels,err,sizeof(err)));
    float mass=motion.mass;int velocity=body.body.vy_fp;
    printf("Known cached stone x%u: pixels%u mass%.6f vy%d\n",world_x,pixels,mass,velocity);
    if(!camera_chunk && (world_x==60 || world_x==124)) {
        unsigned row=(unsigned)(body.body.y_fp>>16)+8u;
        int floor_chunk=(world_x+4u)/64u;
        check_cached_contact_ownership(gpu,chunk,row,floor_chunk,(dc_chunk_coord_t){0,0});
        if(world_x==124) {
            ASSERT_TRUE(dc_gpu_set_page(gpu,0,0,UINT32_MAX,err,sizeof(err)));
            dc_chunk_coord_t distant={INT64_C(1)<<40,-(INT64_C(1)<<40)};
            check_cached_contact_ownership(gpu,chunk,row,floor_chunk,distant);
            check_cached_body_support(gpu,body,distant);
        }
    }
    if(camera_chunk==1) {
        memset(chunk,0,sizeof(*chunk));
        chunk->coord.x=17;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
        dc_gpu_world_body_t frozen;
        ASSERT_TRUE(dc_gpu_read_world_body(gpu,body.body.id,&frozen,err,sizeof(err)));
        ASSERT_EQ(frozen.chunk.x,body.chunk.x);
        ASSERT_EQ(frozen.chunk.y,body.chunk.y);
        ASSERT_EQ(frozen.body.x_fp,body.body.x_fp);
        ASSERT_EQ(frozen.body.y_fp,body.body.y_fp);
        ASSERT_EQ(frozen.body.vy_fp,body.body.vy_fp);
    }
    dc_gpu_destroy(gpu);free(chunk);
    ASSERT_EQ(pixels,128u);ASSERT_TRUE(fabsf(mass-128.f*2.7f)<.002f);
    ASSERT_EQ(solver_body.x_fp,expected_x);
    ASSERT_EQ(solver_body.y_fp,expected_y);
    ASSERT_EQ(velocity,4*16384);PASS();
}
static void test_onscreen_known_stone_advances(void) {cached_stone(20,0);}
static void test_partly_offscreen_known_stone_advances(void) {cached_stone(60,0);}
static void test_wholly_offscreen_known_stone_across_cached_seam_advances(void) {cached_stone(124,0);}
static void test_left_cached_reference_advances(void) {cached_stone(60,1);}
int main(int argc,char **argv) {
    if(argc==2 && strcmp(argv[1],"--cached")==0) {
        RUN(test_wholly_offscreen_known_stone_across_cached_seam_advances);
        return g_fail!=0;
    }
    if(argc==2 && strcmp(argv[1],"--left")==0) {
        RUN(test_left_cached_reference_advances);
        return g_fail!=0;
    }
    if(argc==2 && strcmp(argv[1],"--seam")==0) {
        RUN(test_partly_offscreen_known_stone_advances);
        return g_fail!=0;
    }
    RUN(test_onscreen_known_stone_advances);
    RUN(test_partly_offscreen_known_stone_advances);
    RUN(test_left_cached_reference_advances);
    RUN(test_wholly_offscreen_known_stone_across_cached_seam_advances);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
