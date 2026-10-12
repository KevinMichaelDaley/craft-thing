#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void test_gentle_stone_contact_stays_intact(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    for (int x = 90; x < 98; x++)
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 32, 0, DC_MATERIAL_STONE,
                                          err, sizeof(err)));
    dc_gpu_body_t body = {.x_fp = 90 << 16, .y_fp = (23 << 16) + 49152,
                         .width = 8, .height = 8, .id = 100, .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, body, err, sizeof(err)));
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 100, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabs(motion.mass - 172.8) < .001);
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1,
                                         &motion, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}

static void test_hard_stone_contact_fractures_conservatively(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    for (int x = 90; x < 98; x++)
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 32, 0, DC_MATERIAL_STONE,
                                          err, sizeof(err)));
    dc_gpu_body_t body = {.x_fp = 90 << 16, .y_fp = 20 << 16,
                         .width = 8, .height = 8, .vy_fp = 4 << 16,
                         .id = 100, .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, body, err, sizeof(err)));
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 100, &a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1,
                                        &b, err, sizeof(err)));
    ASSERT_TRUE(fabs(a.mass + b.mass - 172.8) < .001);
    ASSERT_TRUE(a.mass > 0 && b.mass > 0);
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 2,
                                         &b, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_gentle_stone_contact_stays_intact);
    RUN(test_hard_stone_contact_fractures_conservatively);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
