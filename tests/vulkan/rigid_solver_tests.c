#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/gpu.h"
#ifdef DC_QUARTER_NATIVE_VIEW
#include "../../src/app/view_config.h"
#endif

static int g_pass, g_fail;
#define RUN(fn)                                                                                    \
    do {                                                                                           \
        printf("RUN  %s\n", #fn);                                                                  \
        fn();                                                                                      \
        printf("OK   %s\n", #fn);                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                                             \
    do {                                                                                           \
        if (!(e)) {                                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e);                                    \
            g_fail++;                                                                              \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *grid(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk)
        return NULL;
#ifdef DC_QUARTER_NATIVE_VIEW
    uint32_t width = SIM_WIDTH, height = SIM_HEIGHT;
#else
    uint32_t width = 128, height = 128;
#endif
    uint32_t cols = (width + 63) / 64, rows = (height + 63) / 64;
    bool okay =
        dc_gpu_create(&gpu, width, height, "build/shaders/pattern.comp.spv", err, sizeof(err));
    for (uint32_t s = 0; okay && s < cols * rows; ++s) {
        memset(chunk, 0, sizeof(*chunk));
        if (s / cols == 1)
            for (uint32_t x = 0; x < 64; ++x)
                chunk->cells[32 * 64 + x].material = DC_MATERIAL_STONE;
        okay = dc_gpu_upload_chunk(gpu, s, chunk, err, sizeof(err)) &&
               dc_gpu_set_page(gpu, s % cols, s / cols, s, err, sizeof(err));
    }
    free(chunk);
    if (!okay) {
        fprintf(stderr, "%s\n", err);
        dc_gpu_destroy(gpu);
        return NULL;
    }
    return gpu;
}
static dc_gpu_world_body_t box(uint32_t id, int x, int y, uint32_t w, uint32_t h) {
    return (dc_gpu_world_body_t){.chunk = {x / 64, y / 64},
                                 .body = {.x_fp = (x % 64) << 16,
                                          .y_fp = (y % 64) << 16,
                                          .width = w,
                                          .height = h,
                                          .id = id,
                                          .active = 1}};
}
static float xof(dc_gpu_world_body_t b) { return (float)b.chunk.x * 64 + b.body.x_fp / 65536.f; }
static float yof(dc_gpu_world_body_t b) { return (float)b.chunk.y * 64 + b.body.y_fp / 65536.f; }
static dc_gpu_body_shape_t rectangle(uint32_t material, int w, int h) {
    return (dc_gpu_body_shape_t){
        .count = 4,
        .material = material,
        .vertices = {{0, 0}, {w << 16, 0}, {w << 16, h << 16}, {0, h << 16}}};
}

static void test_material_mass_inertia_and_invalid_motion(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    for (uint32_t m = 1; m <= 2; ++m) {
        dc_gpu_body_shape_t shape = rectangle(m, 4, 4);
        ASSERT_TRUE(
            dc_gpu_spawn_convex_body(g, box(m, 20 * m, 20, 4, 4), &shape, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_set_body_origin(g, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    for (uint32_t m = 1; m <= 2; ++m) {
        dc_gpu_body_motion_t p;
        ASSERT_TRUE(dc_gpu_read_body_motion(g, m, &p, err, sizeof(err)));
        float density = m == 1 ? 2.7f : .6f;
        ASSERT_TRUE(fabsf(p.density - density) < .0001f);
        ASSERT_TRUE(fabsf(p.mass - 16 * density) < .001f);
        ASSERT_TRUE(fabsf(p.inertia - p.mass * 32 / 12) < .001f);
        ASSERT_EQ(p.center_x, 2.f);
        ASSERT_EQ(p.center_y, 2.f);
    }
    ASSERT_TRUE(!dc_gpu_set_body_motion(g, 1, NAN, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_set_body_motion(g, 1, 0, INFINITY, 0, err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_set_body_motion(g, 1, 0, 0, 4, err, sizeof(err)));
    dc_gpu_destroy(g);
    PASS();
}
static bool stack(dc_gpu_world_body_t out[3], bool reverse) {
    char err[256];
    dc_gpu_t *g = grid();
    if (!g)
        return false;
    bool okay = dc_gpu_set_rigid_solver(g, true);
    for (uint32_t k = 0; okay && k < 3; ++k) {
        uint32_t id = reverse ? 3 - k : k + 1;
        okay = dc_gpu_spawn_world_body(g, box(id, 62, 96 - 4 * id, 4, 4), err, sizeof(err)) &&
               dc_gpu_set_body_motion(g, id, 0, 0, DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err));
    }
    for (uint32_t k = 0; okay && k < 90; ++k)
        okay = dc_gpu_rigid_step(g, err, sizeof(err));
    dc_gpu_rigid_solver_stats_t stats;
    okay = okay && dc_gpu_read_rigid_solver_stats(g, &stats) && stats.active_bodies == 3 &&
           stats.overflow == 0 && stats.iterations > 1 && stats.substeps > 1 &&
           stats.solved_contacts > 0;
    for (uint32_t id = 1; okay && id <= 3; ++id) {
        okay = dc_gpu_read_world_body(g, id, &out[id - 1], err, sizeof(err));
        okay = okay && fabsf(yof(out[id - 1]) - (96 - 4 * id)) < .1f &&
               abs(out[id - 1].body.vy_fp) < 6554 && abs(out[id - 1].body.vx_fp) < 6554;
        if (!okay)
            fprintf(stderr, "stack %u y=%f vy=%f\n", id, yof(out[id - 1]),
                    out[id - 1].body.vy_fp / 65536.f);
    }
    dc_gpu_destroy(g);
    return okay;
}
static void test_stack_settles_and_slot_order_is_reproducible(void) {
    dc_gpu_world_body_t a[3], b[3];
    ASSERT_TRUE(stack(a, false));
    ASSERT_TRUE(stack(b, true));
    ASSERT_EQ(memcmp(a, b, sizeof(a)), 0);
    PASS();
}
static void test_hard_drop_stops_at_thin_floor(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    dc_gpu_world_body_t b = box(9, 62, 70, 4, 4);
    b.body.vy_fp = 4 << 16;
    dc_gpu_body_shape_t shape = rectangle(1, 4, 4);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g, b, &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 9, 0, 0, DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err)));
    for (uint32_t k = 0; k < 60; ++k) {
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(g, 9, &b, err, sizeof(err)));
        ASSERT_TRUE(yof(b) <= 92.1f);
        ASSERT_TRUE(abs(b.body.vy_fp) <= 4 << 16);
    }
    ASSERT_TRUE(abs(b.body.vy_fp) < 6554);
    dc_gpu_destroy(g);
    PASS();
}
static void test_offset_impact_produces_angular_response(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    ASSERT_TRUE(dc_gpu_paint_material(g, 66, 74, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    dc_gpu_world_body_t b = box(11, 61, 70, 6, 2);
    b.body.vy_fp = 3 << 16;
    dc_gpu_body_shape_t shape = rectangle(1, 6, 2);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g, b, &shape, err, sizeof(err)));
    for (uint32_t k = 0; k < 3; ++k)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t p;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 11, &p, err, sizeof(err)));
    ASSERT_TRUE(isfinite(p.angle) && fabsf(p.angle) > .01f && fabsf(p.angle) < 1.5f);
    ASSERT_TRUE(fabsf(p.angular_velocity) <= .5f);
    dc_gpu_destroy(g);
    PASS();
}
static void test_kinematic_support_carries_dynamic_body(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    dc_gpu_world_body_t support = box(1, 58, 80, 12, 2);
    support.body.vx_fp = 32768;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, support, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(
        g, 1, 0, 0, DC_GPU_BODY_KINEMATIC | DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(2, 62, 76, 4, 4), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 2, 0, 0, DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err)));
    for (uint32_t k = 0; k < 16; ++k)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_world_body_t b;
    ASSERT_TRUE(dc_gpu_read_world_body(g, 1, &support, err, sizeof(err)));
    ASSERT_EQ(xof(support), 66.f);
    ASSERT_EQ(yof(support), 80.f);
    ASSERT_TRUE(dc_gpu_read_world_body(g, 2, &b, err, sizeof(err)));
    ASSERT_TRUE(xof(b) > 67.f && fabsf(yof(b) - 76) < .1f);
    dc_gpu_destroy(g);
    PASS();
}
static void test_incomplete_contacts_roll_back_entire_tick(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    dc_gpu_world_body_t before = box(23, 30, 30, 4, 4), after;
    before.body.vx_fp = 1 << 16;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, before, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(29, 30, 30, 4, 4), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_contact_capacity(g, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g, 23, &after, err, sizeof(err)));
    ASSERT_EQ(memcmp(&before, &after, sizeof(before)), 0);
    dc_gpu_rigid_solver_stats_t stats;
    ASSERT_TRUE(dc_gpu_read_rigid_solver_stats(g, &stats));
    ASSERT_TRUE(stats.overflow & 1u);
    ASSERT_EQ(stats.solved_contacts, 0u);
    ASSERT_TRUE(dc_gpu_set_contact_capacity(g, DC_GPU_CONTACT_CAPACITY, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_rigid_solver_stats(g, &stats));
    ASSERT_EQ(stats.overflow, 0u);
    dc_gpu_destroy(g);
    PASS();
}
static void test_angular_state_survives_rebase_and_snapshot(void) {
    char err[256];
    const char *path = "build/solver_motion.bin";
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    dc_chunk_coord_t origin = {(int64_t)1 << 40, -((int64_t)1 << 40)};
    ASSERT_TRUE(dc_gpu_set_body_origin(g, origin, err, sizeof(err)));
    dc_gpu_world_body_t b = box(31, 62, 10, 4, 4);
    b.chunk = origin;
    b.body.vx_fp = 65536;
    dc_gpu_body_shape_t shape = rectangle(2, 4, 4);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g, b, &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 31, .3f, .1f, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_gpu_body_motion_t before, after;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 31, &before, err, sizeof(err)));
    ASSERT_TRUE(fabsf(before.angle - .4f) < .001f);
    uint32_t current, swept;
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 64, 9, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 31u);
    ASSERT_EQ(swept, 31u);
    ASSERT_TRUE(dc_gpu_save_bodies(g, path, err, sizeof(err)));
    ASSERT_TRUE(
        dc_gpu_set_body_origin(g, (dc_chunk_coord_t){-origin.x, -origin.y}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 31, &after, err, sizeof(err)));
    ASSERT_EQ(memcmp(&before, &after, sizeof(before)), 0);
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 64, 9, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    ASSERT_EQ(swept, 0u);
    ASSERT_TRUE(dc_gpu_remove_body(g, 31, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_load_bodies(g, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g, 31, &after, err, sizeof(err)));
    ASSERT_EQ(memcmp(&before, &after, sizeof(before)), 0);
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 64, 9, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 31u);
    ASSERT_EQ(swept, 31u);
    remove(path);
    dc_gpu_destroy(g);
    PASS();
}
static void test_corrected_occupancy_precedes_fluid_and_mpm(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(37, 20, 92, 4, 4), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 37, 0, 0, DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err)));
    dc_gpu_tick_capture_t c;
    ASSERT_TRUE(dc_gpu_tick_capture(g, &c, err, sizeof(err)));
    ASSERT_EQ(c.stages[0].id, DC_GPU_STAGE_RIGID);
    ASSERT_EQ(c.stages[1].id, DC_GPU_STAGE_FLUID);
    ASSERT_EQ(c.stages[2].id, DC_GPU_STAGE_SAND);
    ASSERT_TRUE(c.stages[0].gpu_ns > 0);
    printf("solver GPU time: %.3f ms\n", c.stages[0].gpu_ns / 1e6);
    ASSERT_EQ(c.stages[0].handoff, 37u);
    ASSERT_EQ(c.stages[1].handoff, 38u);
    ASSERT_EQ(c.stages[2].handoff, 39u);
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 20, 92, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 37u);
    dc_gpu_destroy(g);
    PASS();
}
static void test_fractional_boxes_publish_the_full_corrected_footprint(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    dc_gpu_world_body_t b = box(41,20,10,4,4);
    b.body.x_fp += 16384;
    b.body.vx_fp = 32768;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,b,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    uint32_t current,swept;
    ASSERT_TRUE(dc_gpu_read_occupancy(g,24,11,&current,&swept,err,sizeof(err)));
    ASSERT_EQ(current,41u); ASSERT_EQ(swept,41u);
    ASSERT_TRUE(dc_gpu_read_occupancy(g,25,11,&current,&swept,err,sizeof(err)));
    ASSERT_EQ(current,0u); ASSERT_EQ(swept,0u);
    dc_gpu_destroy(g); PASS();
}
int main(void) {
    RUN(test_fractional_boxes_publish_the_full_corrected_footprint);
    RUN(test_material_mass_inertia_and_invalid_motion);
    RUN(test_stack_settles_and_slot_order_is_reproducible);
    RUN(test_hard_drop_stops_at_thin_floor);
    RUN(test_offset_impact_produces_angular_response);
    RUN(test_kinematic_support_carries_dynamic_body);
    RUN(test_incomplete_contacts_roll_back_entire_tick);
    RUN(test_angular_state_survives_rebase_and_snapshot);
    RUN(test_corrected_occupancy_precedes_fluid_and_mpm);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
