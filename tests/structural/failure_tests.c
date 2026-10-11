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
static fr_structural_gpu_graph_t two_members(void) {
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    s.node_count = 3;
    s.member_count = 2;
    s.nodes[1].x_fp = 26 << 16;
    s.nodes[2] = s.nodes[1];
    s.nodes[2].id = 3;
    s.nodes[2].x_fp = 32 << 16;
    s.members[1] = s.members[0];
    s.members[1].id = 2;
    s.members[1].a = 2;
    s.members[1].b = 3;
    s.members[1].material = FR_STRUCTURAL_STONE;
    return s;
}
static void test_sound_mixed_assembly_retains_its_bond(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = two_members();
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    steps(g, 90);
    uint32_t a = FR_STRUCTURAL_BODY_ID_BASE + 1;
    uint32_t b = FR_STRUCTURAL_BODY_ID_BASE + 2;
    dc_gpu_body_motion_t ma, mb;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, a, &ma, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, b, &mb, e, sizeof(e)));
    ASSERT_TRUE(!(ma.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(!(mb.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(fabs(ma.mass - 7.2) < .001);
    ASSERT_TRUE(fabs(mb.mass - 32.4) < .001);
    ASSERT_TRUE(dc_gpu_set_body_motion(g, a, ma.angle, .05f, 0, e, sizeof(e)));
    steps(g, 6);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, a, &ma, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, b, &mb, e, sizeof(e)));
    dc_gpu_body_t ba, bb;
    ASSERT_TRUE(dc_gpu_read_body_id(g, a, &ba, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_body_id(g, b, &bb, e, sizeof(e)));
    double ax = ba.x_fp / 65536. + ma.center_x + 3 * cos(ma.angle);
    double ay = ba.y_fp / 65536. + ma.center_y + 3 * sin(ma.angle);
    double bx = bb.x_fp / 65536. + mb.center_x - 3 * cos(mb.angle);
    double by = bb.y_fp / 65536. + mb.center_y - 3 * sin(mb.angle);
    ASSERT_TRUE(hypot(ax - bx, ay - by) < .025);
    ASSERT_TRUE(fabs(ma.angle - mb.angle) < .01);
    dc_gpu_destroy(g);
    PASS();
}
int main(void) {
    RUN(test_support_loss_grace_then_conservative_fall);
    RUN(test_unknown_does_not_age_support_loss);
    RUN(test_sound_mixed_assembly_retains_its_bond);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
