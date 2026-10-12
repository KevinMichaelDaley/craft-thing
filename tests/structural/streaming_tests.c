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

int main(void) {
    RUN(test_cold_diagonal_footprint_freezes_support_loss_clock);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
