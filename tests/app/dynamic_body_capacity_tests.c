#define main material_cut_fixture_main
#include "material_cut_buoyancy_tests.c"
#undef main

static void test_material_painting_grows_beyond_sixty_four_live_bodies(void) {
    enum { REQUESTED_BODIES=320 };
    char directory[]="build/ui_dynamic_bodies_XXXXXX",err[256]={0};
    ASSERT_TRUE(mkdtemp(directory));
    printf("Actual sandbox world: %s\n",directory);
    ASSERT_TRUE(seed_mainland_pond_layout(directory,DC_MATERIAL_AIR,false));
    dc_level_view_t *view=dc_level_view_create(directory,314,err,sizeof(err));
    ASSERT_TRUE(view);ASSERT_TRUE(dc_level_view_set_spring_enabled(view,false));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    for(unsigned stamp=0;stamp<REQUESTED_BODIES;stamp++) {
        /* Paint upward: earlier falling bodies stay below the next row.
         * The gameplay brush intentionally ignores already occupied pixels. */
        unsigned x=20u+(stamp%30u)*14u,y=130u-(stamp/30u)*12u;
        ASSERT_TRUE(dc_level_view_paint(view,x,y,1,DC_MATERIAL_STONE,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_step(view,err,sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view,err,sizeof(err)));
    }
    unsigned found=0,exact=0;
    for(unsigned serial=1;serial<=REQUESTED_BODIES;serial++) {
        dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
        if(dc_level_view_body_state(view,0x80000000u+serial,&body,&motion,err,sizeof(err))) {
            found++;exact+=fabsf(motion.mass-5.f*2.7f)<.001f&&
                (motion.flags&DC_GPU_BODY_KINEMATIC)==0u;
        }
    }
    printf("Actual material sandbox: requested%u live%u exact stone mass/dynamic%u\n",REQUESTED_BODIES,found,exact);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    ASSERT_EQ(found,REQUESTED_BODIES);ASSERT_EQ(exact,REQUESTED_BODIES);
    view=dc_level_view_create(directory,314,err,sizeof(err));
    ASSERT_TRUE(view);ASSERT_TRUE(dc_level_view_set_spring_enabled(view,false));
    ASSERT_TRUE(dc_level_view_wait_visible(view,120000,err,sizeof(err)));
    found=0;exact=0;
    for(unsigned serial=1;serial<=REQUESTED_BODIES;serial++) {
        dc_gpu_world_body_t body;dc_gpu_body_motion_t motion;
        if(dc_level_view_body_state(view,0x80000000u+serial,&body,&motion,err,sizeof(err))) {
            found++;exact+=fabsf(motion.mass-5.f*2.7f)<.001f&&
                (motion.flags&DC_GPU_BODY_KINEMATIC)==0u;
        }
    }
    printf("Reloaded material sandbox: requested%u live%u exact stone mass/dynamic%u\n",REQUESTED_BODIES,found,exact);
    ASSERT_TRUE(dc_level_view_destroy(view,err,sizeof(err)));
    ASSERT_EQ(found,REQUESTED_BODIES);ASSERT_EQ(exact,REQUESTED_BODIES);PASS();
}
int main(void) {
    RUN(test_material_painting_grows_beyond_sixty_four_live_bodies);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
