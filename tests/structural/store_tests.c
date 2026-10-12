#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static const char *snapshot = "build/structural_snapshot.bin";

static void test_cold_checkpoint_survives_another_checkpoint(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g, 85, 90, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = NULL;
    ASSERT_TRUE(dc_gpu_create(&g, 128, 128, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
#ifdef DC_QUARTER_NATIVE_VIEW
    const unsigned slot = 11;
#else
    const unsigned slot = 3;
#endif
    ASSERT_TRUE(dc_gpu_download_chunk(g, slot, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[(90 % 64) * 64 + 85 % 64].material, DC_MATERIAL_STONE);
    free(chunk); remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_snapshot_preserves_crumble_particles_and_world_cells(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *before = calloc(1, sizeof(*before));
    dc_chunk_t *after = calloc(1, sizeof(*after));
    ASSERT_TRUE(before && after);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, before, err, sizeof(err)));
    ASSERT_TRUE(before->particle_count > 0);
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, after, err, sizeof(err)));
    ASSERT_EQ(after->particle_count, before->particle_count);
    ASSERT_TRUE(memcmp(after->particles, before->particles, sizeof(after->particles)) == 0);
    ASSERT_TRUE(memcmp(after->cells, before->cells, sizeof(after->cells)) == 0);
    free(before); free(after); remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_snapshot_resumes_exact_overload_damage(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 6; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t saved, continued, restored;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &saved));
    ASSERT_TRUE(saved.damage > 0 && saved.damage < 1);
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &continued));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &restored));
    ASSERT_EQ(restored.damage, saved.damage);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &restored));
    ASSERT_TRUE(fabs(restored.damage - continued.damage) < .000001);
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_snapshot_resumes_confirmed_support_loss_ticks(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 30; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    for (unsigned i = 0; i < 59; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion, err, sizeof(err)));
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &motion, err, sizeof(err)));
    ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_snapshot_preserves_descendants_and_output_serial(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 18; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t before, after;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &before, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &after, err, sizeof(err)));
    ASSERT_EQ(before.mass, after.mass);
    fr_structural_gpu_node_state_t history;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &history));
    ASSERT_EQ(history.id, 1);
    ASSERT_EQ(history.generation, 1);
    ASSERT_EQ(history.state, FR_STRUCTURAL_CONVERTED);
    fr_structural_gpu_component_state_t component;
    ASSERT_TRUE(fr_gpu_read_structure_component(g, 1, &component));
    ASSERT_EQ(component.id, 1);
    ASSERT_EQ(component.state, FR_STRUCTURAL_CONVERTED);
    ASSERT_TRUE(dc_gpu_remove_body(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, err, sizeof(err)));
    s = beam(70); s.nodes[0].id = 3; s.nodes[1].id = 4;
    s.members[0].id = 2; s.members[0].a = 3; s.members[0].b = 4;
    s.members[0].material = FR_STRUCTURAL_STONE;
    s.anchors[0].id = 2; s.anchors[0].node = 3;
    ASSERT_TRUE(dc_gpu_paint_material(g, 70, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 18; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 2, &after, err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &after, err, sizeof(err)));
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_saturated_pending_snapshot_commits_exactly_once(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 63; i++) {
        dc_gpu_body_t other = {.x_fp = (70 + i % 8 * 3) << 16,
                              .y_fp = (70 + i / 8 * 3) << 16,
                              .width = 1, .height = 1, .id = i + 1, .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(g, other, err, sizeof(err)));
    }
    for (unsigned i = 0; i < 18; i++) ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_PENDING_CONVERSION);
    ASSERT_TRUE(fr_gpu_save_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(g, 63, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &b, err, sizeof(err)));
    ASSERT_TRUE(fabs(a.mass + b.mass - 64.8) < .001);
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 2, &b, err, sizeof(err)));
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_corrupt_load_leaves_existing_pool_unchanged(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    FILE *file = fopen(snapshot, "wb"); ASSERT_TRUE(file);
    ASSERT_TRUE(fputc('x', file) != EOF); ASSERT_EQ(fclose(file), 0);
    dc_gpu_body_t ordinary = {.x_fp = 80 << 16, .y_fp = 80 << 16,
                             .width = 1, .height = 1, .id = 9, .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, ordinary, err, sizeof(err)));
    ASSERT_TRUE(!fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_body_t preserved;
    ASSERT_TRUE(dc_gpu_read_body_id(g, 9, &preserved, err, sizeof(err)));
    ASSERT_EQ(preserved.x_fp, ordinary.x_fp);
    ASSERT_EQ(preserved.y_fp, ordinary.y_fp);
    ASSERT_EQ(preserved.active, 1);
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

static void test_legacy_body_world_still_loads_without_a_graph(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    dc_gpu_body_t body = {.x_fp = 80 << 16, .y_fp = 80 << 16,
                         .width = 1, .height = 1, .id = 7, .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, body, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_save_bodies(g, snapshot, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(g, 7, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(g, snapshot, err, sizeof(err)));
    dc_gpu_body_t restored;
    ASSERT_TRUE(dc_gpu_read_body_id(g, 7, &restored, err, sizeof(err)));
    ASSERT_EQ(restored.width, 1);
    ASSERT_EQ(restored.active, 1);
    ASSERT_TRUE(!fr_gpu_read_structure_member(g, 1, &(fr_structural_gpu_member_state_t){0}));
    remove(snapshot); dc_gpu_destroy(g); PASS();
}

int main(void) {
    RUN(test_cold_checkpoint_survives_another_checkpoint);
    RUN(test_snapshot_preserves_crumble_particles_and_world_cells);
    RUN(test_snapshot_resumes_exact_overload_damage);
    RUN(test_snapshot_resumes_confirmed_support_loss_ticks);
    RUN(test_snapshot_preserves_descendants_and_output_serial);
    RUN(test_saturated_pending_snapshot_commits_exactly_once);
    RUN(test_corrupt_load_leaves_existing_pool_unchanged);
    RUN(test_legacy_body_world_still_loads_without_a_graph);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
