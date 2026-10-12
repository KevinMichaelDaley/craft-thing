#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void test_checkpoint_preserves_attachment_damage_and_cut(void) {
    const char *path = "build/structural_cut_checkpoint.bin";
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(20);
    graph.anchors[0].moment_capacity = 25920;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    for (unsigned i = 0; i < 44; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, path, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, path, err, sizeof(err)));
    for (unsigned i = 0; i < 2; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_node_state_t node;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &node));
    ASSERT_EQ(node.state, FR_STRUCTURAL_UNSUPPORTED);
    ASSERT_TRUE(fr_gpu_save_structure(g, path, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, path, err, sizeof(err)));
    for (unsigned i = 0; i < 88; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion, err, sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    remove(path); dc_gpu_destroy(g); PASS();
}

static void test_world_rebase_preserves_member_force_and_stress(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(70);
    ASSERT_TRUE(dc_gpu_paint_material(g, 70, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t before, after;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &before));
    ASSERT_TRUE(dc_gpu_set_body_origin(g, (dc_chunk_coord_t){1, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g, 0, 1, 3, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &after));
    ASSERT_EQ(before.state, after.state);
    ASSERT_TRUE(fabs(before.axial - after.axial) <= .0001 * (1 + fabs(before.axial)));
    ASSERT_TRUE(fabs(before.shear - after.shear) <= .0001 * (1 + fabs(before.shear)));
    ASSERT_TRUE(fabs(before.moment - after.moment) <= .0001 * (1 + fabs(before.moment)));
    ASSERT_TRUE(fabs(before.utilization - after.utilization) < .0001);
    ASSERT_EQ(before.damage, after.damage);
    dc_gpu_destroy(g); PASS();
}

static void test_live_descendant_prevents_reusing_its_root_generation(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(20);
    graph.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    for (unsigned i = 0; i < 18; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t child;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &child, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &(fr_structural_gpu_graph_t){0}, err, sizeof(err)));
    graph = beam(70); graph.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &child, err, sizeof(err)));
    ASSERT_TRUE(child.mass > 0);
    dc_gpu_destroy(g); PASS();
}

int main(void) {
    RUN(test_checkpoint_preserves_attachment_damage_and_cut);
    RUN(test_world_rebase_preserves_member_force_and_stress);
    RUN(test_live_descendant_prevents_reusing_its_root_generation);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
