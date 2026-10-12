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
static void
test_sound_detachment_hands_wood_and_stone_to_density_derived_water_motion(
    void) {
    char err[256] = {0};
    for (unsigned material = 0; material < 2; material++) {
        dc_gpu_t *gpu = grid();
        dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
        ASSERT_TRUE(gpu && chunk);
        ASSERT_TRUE(dc_gpu_paint_material(gpu, 20, 41, 0, DC_MATERIAL_STONE,
                                          err, sizeof(err)));
        fr_structural_gpu_graph_t graph = beam(20);
        graph.nodes[1].x_fp = 26 << 16;
        graph.members[0].material =
            material ? FR_STRUCTURAL_STONE : FR_STRUCTURAL_WOOD;
        ASSERT_TRUE(fr_gpu_place_structure(gpu, &graph, err, sizeof(err)));
        for (unsigned y = 0; y < 64; y++)
            for (unsigned x = 0; x < 64; x++) {
                dc_cell_t *cell = &chunk->cells[y * 64 + x];
                if (x == 0 || x == 63 || y == 63 || (x == 20 && y == 41))
                    cell->material = DC_MATERIAL_STONE;
                else if (y >= 20)
                    cell->fluid_mass = DC_FLUID_FULL;
            }
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_rigid_solver(gpu, true));
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
        fr_structural_gpu_member_state_t state;
        ASSERT_TRUE(fr_gpu_read_structure_member(gpu, 1, &state));
        ASSERT_EQ(state.state, FR_STRUCTURAL_SUPPORTED);
        ASSERT_EQ(state.damage, 0);
        ASSERT_TRUE(dc_gpu_paint_material(gpu, 20, 41, 0, DC_MATERIAL_AIR, err,
                                          sizeof(err)));
        unsigned grace = 0;
        while (grace < 240) {
            ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
            grace++;
            ASSERT_TRUE(fr_gpu_read_structure_member(gpu, 1, &state));
            if (state.state == FR_STRUCTURAL_CONVERTED)
                break;
        }
        unsigned expected_grace = (unsigned)lround(
            fr_structural_profile(graph.members[0].material)->grace_seconds *
            60);
        ASSERT_TRUE(grace >= expected_grace && grace <= expected_grace + 1);
        ASSERT_EQ(state.damage, 0);
        for (unsigned frame = 0; frame < 600; frame++)
            ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        dc_gpu_body_t body;
        dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_body_id(gpu, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                        &body, err, sizeof(err)));
        ASSERT_TRUE(
            dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err)));
        ASSERT_TRUE(!(motion.flags & DC_GPU_BODY_KINEMATIC));
        ASSERT_TRUE(fabs(motion.mass - (material ? 32.4 : 7.2)) < .001);
        printf("Detached wet material %u grace %u y %.4f mass %.4f\n", material,
               grace, body.y_fp / 65536.0, motion.mass);
        ASSERT_TRUE(material ? body.y_fp > 60 << 16
                             : body.y_fp > 18 << 16 && body.y_fp < 20 << 16);
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
        uint64_t water = 0;
        for (unsigned cell = 0; cell < 4096; cell++)
            water += chunk->cells[cell].fluid_mass;
        ASSERT_EQ(water, (43u * 62u - 1u) * (uint64_t)DC_FLUID_FULL);
        free(chunk);
        dc_gpu_destroy(gpu);
    }
    PASS();
}
int main(void) {
    RUN(test_anchored_wood_transfers_material_derived_buoyancy_into_support_graph);
    RUN(test_sound_detachment_hands_wood_and_stone_to_density_derived_water_motion);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
