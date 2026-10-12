#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include "../vulkan/compound_fixture.h"

static void test_mixed_impact_fracture_work_follows_each_material(void) {
    char err[256];
    dc_gpu_t *gpu = grid();
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(gpu));
    for (int x = 90; x < 98; x++)
        ASSERT_TRUE(dc_gpu_paint_material(gpu, x, 32, 0, DC_MATERIAL_STONE, err,
                                          sizeof(err)));
    dc_gpu_world_body_t body = fixture_body(100, 90, 20);
    body.body.width = 8;
    body.body.height = 8;
    body.body.vy_fp = 4 << 16;
    dc_gpu_compound_shape_t shape = {.count = 2};
    shape.pieces[0] = fixture_rectangle(DC_GPU_BODY_STONE, 0, 0, 4, 8);
    shape.pieces[1] = fixture_rectangle(DC_GPU_BODY_WOOD, 4, 0, 4, 8);
    ASSERT_TRUE(
        dc_gpu_spawn_compound_body(gpu, body, &shape, err, sizeof(err)));
    for (unsigned tick = 0; tick < 4; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    bool born = dc_gpu_read_body_motion(gpu, 100, &a, err, sizeof(err)) &&
                dc_gpu_read_body_motion(gpu, FR_STRUCTURAL_OUTPUT_ID_BASE + 1,
                                        &b, err, sizeof(err));
    dc_gpu_compound_shape_t sa, sb;
    bool composition =
        born && dc_gpu_read_compound_shape(gpu, 100, &sa, err, sizeof(err)) &&
        dc_gpu_read_compound_shape(gpu, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &sb,
                                   err, sizeof(err));
    dc_gpu_impact_fracture_state_t state;
    bool credited = dc_gpu_read_impact_fracture(gpu, 100, &state);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(born);
    ASSERT_TRUE(composition);
    ASSERT_TRUE(fabsf(a.mass + b.mass - 105.6f) < .001f);
    ASSERT_EQ(sa.count, 2u);
    ASSERT_EQ(sb.count, 2u);
    ASSERT_EQ(sa.pieces[0].material, DC_GPU_BODY_STONE);
    ASSERT_EQ(sa.pieces[1].material, DC_GPU_BODY_WOOD);
    ASSERT_EQ(sb.pieces[0].material, DC_GPU_BODY_STONE);
    ASSERT_EQ(sb.pieces[1].material, DC_GPU_BODY_WOOD);
    ASSERT_TRUE(credited);
    ASSERT_TRUE(fabsf(state.fracture_work - (4 * 144.f + 4 * 12000.f * .15f)) <
                .01f);
    ASSERT_TRUE(state.fracture_work <= state.available_work);
    PASS();
}
int main(void) {
    RUN(test_mixed_impact_fracture_work_follows_each_material);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
