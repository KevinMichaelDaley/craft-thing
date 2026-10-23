#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include "../vulkan/compound_fixture.h"

static void test_gentle_stone_contact_stays_intact(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(g));
    for (int x = 90; x < 98; x++)
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 32, 0, DC_MATERIAL_STONE, err,
                                          sizeof(err)));
    dc_gpu_body_t body = {.x_fp = 90 << 16,
                          .y_fp = (23 << 16) + 49152,
                          .width = 8,
                          .height = 8,
                          .id = 100,
                          .active = 1};
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
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 32, 0, DC_MATERIAL_STONE, err,
                                          sizeof(err)));
    dc_gpu_body_t body = {.x_fp = 90 << 16,
                          .y_fp = 20 << 16,
                          .width = 8,
                          .height = 8,
                          .vy_fp = 4 << 16,
                          .id = 100,
                          .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, body, err, sizeof(err)));
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 100, &a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &b,
                                        err, sizeof(err)));
    ASSERT_TRUE(fabs(a.mass + b.mass - 172.8) < .001);
    ASSERT_TRUE(a.mass > 0 && b.mass > 0);
    dc_gpu_impact_fracture_state_t state;
    ASSERT_TRUE(dc_gpu_read_impact_fracture(g, 100, &state));
    ASSERT_TRUE(!state.pending);
    ASSERT_TRUE(state.available_work > 0 && state.fracture_work > 0);
    ASSERT_TRUE(state.fracture_work <= state.available_work);
    ASSERT_TRUE(fabs(state.fracture_work + state.dissipated_work -
                     state.available_work) < .01);
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 2,
                                         &b, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}

static void test_full_initial_page_grows_and_preserves_all_impact_owners(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g && dc_gpu_enable_impact_fracture(g));
    for (int x = 90; x < 98; x++)
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 32, 0, DC_MATERIAL_STONE, err,
                                          sizeof(err)));
    dc_gpu_body_t body = {.x_fp = 90 << 16,
                          .y_fp = 20 << 16,
                          .width = 8,
                          .height = 8,
                          .vy_fp = 4 << 16,
                          .id = 100,
                          .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, body, err, sizeof(err)));
    for (unsigned i = 0; i < 63; i++) {
        dc_gpu_body_t other = {.x_fp = (5 + i % 8 * 3) << 16,
                               .y_fp = (60 + i / 8 * 3) << 16,
                               .width = 1,
                               .height = 1,
                               .id = i + 1,
                               .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(g, other, err, sizeof(err)));
    }
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 100, &a, err, sizeof(err)));
    dc_gpu_impact_fracture_state_t state;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, &b,
                                        err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 100, &a, err, sizeof(err)));
    ASSERT_TRUE(fabs(a.mass + b.mass - 172.8) < .001);
    ASSERT_TRUE(dc_gpu_read_impact_fracture(g, 100, &state));
    ASSERT_TRUE(!state.pending);
    for(unsigned i=1;i<=63;i++) {
        dc_gpu_body_t retained;
        ASSERT_TRUE(dc_gpu_read_body_id(g,i,&retained,err,sizeof(err)));
        ASSERT_EQ(retained.id,i);
    }
    dc_gpu_destroy(g);
    PASS();
}

static void test_two_concave_impacts_preserve_connected_output_mass(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g && dc_gpu_enable_impact_fracture(g));
    dc_gpu_compound_shape_t l = fixture_l(), u = fixture_u();
    for (unsigned i = 0; i < u.count; i++)
        u.pieces[i].material = DC_GPU_BODY_STONE;
    for (int x = 20; x < 76; x++)
        ASSERT_TRUE(dc_gpu_paint_material(g, x, 40, 0, DC_MATERIAL_STONE, err,
                                          sizeof(err)));
    dc_gpu_world_body_t a = fixture_body(100, 20, 20),
                        b = fixture_body(101, 60, 20);
    a.body.vy_fp = b.body.vy_fp = 4 << 16;
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g, a, &l, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g, b, &u, err, sizeof(err)));
    for (unsigned i = 0; i < 4; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    float masses[5];
    const uint32_t ids[] = {100, FR_STRUCTURAL_OUTPUT_ID_BASE + 1, 101,
                            FR_STRUCTURAL_OUTPUT_ID_BASE + 2,
                            FR_STRUCTURAL_OUTPUT_ID_BASE + 3};
    for (unsigned i = 0; i < 5; i++) {
        dc_gpu_body_motion_t motion;
        ASSERT_TRUE(
            dc_gpu_read_body_motion(g, ids[i], &motion, err, sizeof(err)));
        masses[i] = motion.mass;
    }
    ASSERT_TRUE(fabs(masses[0] + masses[1] - 302.4) < .001);
    ASSERT_TRUE(fabs(masses[2] + masses[3] + masses[4] - 432) < .001);
    dc_gpu_body_motion_t extra;
    ASSERT_TRUE(!dc_gpu_read_body_motion(g, FR_STRUCTURAL_OUTPUT_ID_BASE + 4,
                                         &extra, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_gentle_stone_contact_stays_intact);
    RUN(test_hard_stone_contact_fractures_conservatively);
    RUN(test_full_initial_page_grows_and_preserves_all_impact_owners);
    RUN(test_two_concave_impacts_preserve_connected_output_mass);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
