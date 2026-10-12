#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void
test_anchored_wood_transfers_material_derived_buoyancy_into_support_graph(
    void) {
    char err[256];
    dc_gpu_t *gpu = grid();
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 20, 41, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)));
    fr_structural_gpu_graph_t graph = beam(20);
    ASSERT_TRUE(fr_gpu_place_structure(gpu, &graph, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
            chunk->cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    chunk->cells[41 * 64 + 20].material = DC_MATERIAL_STONE;
    chunk->cells[41 * 64 + 20].fluid_mass = 0;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    fr_structural_gpu_component_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_component(gpu, 1, &state));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &motion, err, sizeof(err)));
    printf("submerged wood external %.6f reaction %.6f state %u\n",
           state.external[1], state.reaction[1], state.state);
    dc_gpu_destroy(gpu);
    free(chunk);
    ASSERT_EQ(state.state, FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(fabsf(state.external[1] + 8640.f) < 1.f);
    ASSERT_TRUE(fabsf(state.reaction[1] - 8640.f) < 1.f);
    ASSERT_TRUE(fabsf(motion.mass - 14.4f) < .001f);
    ASSERT_TRUE(motion.flags & DC_GPU_BODY_KINEMATIC);
    PASS();
}
int main(void) {
    RUN(test_anchored_wood_transfers_material_derived_buoyancy_into_support_graph);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
