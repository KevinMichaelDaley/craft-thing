#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void test_cold_diagonal_footprint_freezes_support_loss_clock(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(63);
    s.nodes[0].y_fp = 60 << 16;
    s.nodes[1].chunk = (dc_chunk_coord_t){1, 1};
    s.nodes[1].x_fp = 6 << 16;
    s.nodes[1].y_fp = 3 << 16;
    s.anchors[0].x = 62;
    s.anchors[0].y = 58;
    ASSERT_TRUE(dc_gpu_paint_material(g, 62, 58, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_node_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 62, 58, 0, DC_MATERIAL_AIR, err, sizeof(err)));
    for (unsigned i = 0; i < 10; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g, 1, 0, UINT32_MAX, err, sizeof(err)));
    for (unsigned i = 0; i < 100; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_UNKNOWN);
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_gpu_set_page(g, 1, 0, 1, err, sizeof(err)));
    for (unsigned i = 0; i < 79; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    dc_gpu_destroy(g);
    PASS();
}

static void test_reordering_members_preserves_damage_by_identity(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20), other = beam(70);
    s.members[0].material = FR_STRUCTURAL_STONE;
    s.node_count = 4;
    s.member_count = 2;
    s.anchor_count = 2;
    other.nodes[0].id = 3;
    other.nodes[1].id = 4;
    other.members[0].id = 2;
    other.members[0].a = 3;
    other.members[0].b = 4;
    other.anchors[0].id = 2;
    other.anchors[0].node = 3;
    s.nodes[2] = other.nodes[0];
    s.nodes[3] = other.nodes[1];
    s.members[1] = other.members[0];
    s.anchors[1] = other.anchors[0];
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g, 70, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 6; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t before, stone, wood;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &before));
    ASSERT_TRUE(before.damage > 0);
    fr_structural_gpu_member_t swap = s.members[0];
    s.members[0] = s.members[1];
    s.members[1] = swap;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &stone));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 2, &wood));
    ASSERT_EQ(stone.damage, before.damage);
    ASSERT_EQ(wood.damage, 0);
    dc_gpu_destroy(g);
    PASS();
}

static void test_new_member_generation_starts_with_fresh_damage(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 6; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_TRUE(state.damage > 0);
    s.members[0].generation = 2;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.damage, 0);
    dc_gpu_destroy(g);
    PASS();
}

static void test_republishing_unchanged_graph_does_not_reset_grace(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 30; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 59; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    dc_gpu_destroy(g);
    PASS();
}

static void test_stale_generation_cannot_replace_current_identity(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].generation = 2;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    s.members[0].generation = 1;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_cold_diagonal_footprint_freezes_support_loss_clock);
    RUN(test_reordering_members_preserves_damage_by_identity);
    RUN(test_new_member_generation_starts_with_fresh_damage);
    RUN(test_republishing_unchanged_graph_does_not_reset_grace);
    RUN(test_stale_generation_cannot_replace_current_identity);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
