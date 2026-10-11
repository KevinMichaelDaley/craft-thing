#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void steps(dc_gpu_t *g, unsigned count) {
    char e[256];
    for (unsigned i = 0; i < count; i++)
        if (!dc_gpu_rigid_step(g, e, sizeof(e))) {
            printf("step failed: %s\n", e);
            g_fail++;
            return;
        }
}
static void test_support_loss_grace_then_conservative_fall(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_gpu_body_motion_t before, after;
    uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + 1;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &before, e, sizeof(e)));
    steps(g, 89);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &after, e, sizeof(e)));
    ASSERT_TRUE(after.flags & DC_GPU_BODY_KINEMATIC);
    steps(g, 1);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &after, e, sizeof(e)));
    ASSERT_TRUE(!(after.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(fabs(after.mass - before.mass) < .0001);
    ASSERT_TRUE(fabs(after.inertia - before.inertia) < .0001);
    ASSERT_TRUE(after.angular_velocity == 0);
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(g, id, &body, e, sizeof(e)));
    int y = body.y_fp;
    steps(g, 4);
    ASSERT_TRUE(dc_gpu_read_body_id(g, id, &body, e, sizeof(e)));
    ASSERT_TRUE(body.y_fp > y);
    dc_gpu_destroy(g);
    PASS();
}
static void test_unknown_does_not_age_support_loss(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    steps(g, 60);
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, UINT32_MAX, e, sizeof(e)));
    steps(g, 95);
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 0, e, sizeof(e)));
    steps(g, 29);
    dc_gpu_body_motion_t m;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &m, e, sizeof(e)));
    ASSERT_TRUE(m.flags & DC_GPU_BODY_KINEMATIC);
    steps(g, 1);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &m, e, sizeof(e)));
    ASSERT_TRUE(!(m.flags & DC_GPU_BODY_KINEMATIC));
    dc_gpu_destroy(g);
    PASS();
}
int main(void) {
    RUN(test_support_loss_grace_then_conservative_fall);
    RUN(test_unknown_does_not_age_support_loss);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
