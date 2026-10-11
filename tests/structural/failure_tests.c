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
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &m,
                                        e, sizeof(e)));
    ASSERT_TRUE(m.flags & DC_GPU_BODY_KINEMATIC);
    steps(g, 1);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &m,
                                        e, sizeof(e)));
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
    double initial_angular_momentum = ma.inertia * .05;
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
    double total = ma.mass + mb.mass;
    double acx = ba.x_fp / 65536. + ma.center_x;
    double acy = ba.y_fp / 65536. + ma.center_y;
    double bcx = bb.x_fp / 65536. + mb.center_x;
    double bcy = bb.y_fp / 65536. + mb.center_y;
    double cx = (ma.mass * acx + mb.mass * bcx) / total;
    double cy = (ma.mass * acy + mb.mass * bcy) / total;
    double avx = ba.vx_fp / 65536., avy = ba.vy_fp / 65536.;
    double bvx = bb.vx_fp / 65536., bvy = bb.vy_fp / 65536.;
    double px = ma.mass * avx + mb.mass * bvx;
    double py = ma.mass * avy + mb.mass * bvy;
    double angular =
        ma.inertia * ma.angular_velocity + mb.inertia * mb.angular_velocity;
    angular += ma.mass * ((acx - cx) * avy - (acy - cy) * avx);
    angular += mb.mass * ((bcx - cx) * bvy - (bcy - cy) * bvx);
    printf("assembly momentum: %.6f %.6f angular %.6f expected %.6f\n", px, py,
           angular, initial_angular_momentum);
    ASSERT_TRUE(fabs(px) < .02);
    ASSERT_TRUE(fabs(py - total * 6 * .25) < .02);
    ASSERT_TRUE(fabs(angular - initial_angular_momentum) < .03);
    dc_gpu_destroy(g);
    PASS();
}
static void test_full_bond_pool_rolls_back_the_whole_island(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = two_members();
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_set_structure_bond_capacity(g, 0));
    steps(g, 90);
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &a,
                                        e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 2, &b,
                                        e, sizeof(e)));
    ASSERT_TRUE(a.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(b.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(fabs(a.mass + b.mass - 39.6) < .001);
    fr_structural_gpu_component_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_component(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_PENDING_CONVERSION);
    ASSERT_TRUE(fr_gpu_set_structure_bond_capacity(g, 1));
    steps(g, 1);
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &a,
                                        e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 2, &b,
                                        e, sizeof(e)));
    ASSERT_TRUE(!(a.flags & DC_GPU_BODY_KINEMATIC));
    ASSERT_TRUE(!(b.flags & DC_GPU_BODY_KINEMATIC));
    dc_gpu_destroy(g);
    PASS();
}
static void test_overstress_ages_damage_but_unknown_freezes_it(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    steps(g, 10);
    fr_structural_gpu_member_state_t before, after;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &before));
    ASSERT_TRUE(before.damage > .55 && before.damage < .62);
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, UINT32_MAX, e, sizeof(e)));
    steps(g, 20);
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &after));
    ASSERT_EQ(after.state, FR_STRUCTURAL_UNKNOWN);
    ASSERT_EQ(before.damage, after.damage);
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 0, e, sizeof(e)));
    steps(g, 8);
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &after));
    ASSERT_EQ(after.state, FR_STRUCTURAL_CONVERTED);
    ASSERT_EQ(after.damage, 1.0f);
    dc_gpu_destroy(g);
    PASS();
}
int main(void) {
    RUN(test_support_loss_grace_then_conservative_fall);
    RUN(test_unknown_does_not_age_support_loss);
    RUN(test_sound_mixed_assembly_retains_its_bond);
    RUN(test_full_bond_pool_rolls_back_the_whole_island);
    RUN(test_overstress_ages_damage_but_unknown_freezes_it);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
