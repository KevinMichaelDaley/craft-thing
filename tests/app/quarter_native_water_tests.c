#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static uint64_t source_mass(const dc_chunk_t *chunk) {
    uint64_t mass = 0u;
    for (uint32_t y = 0u; y < 10u; ++y)
        for (uint32_t x = 0u; x < 5u; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    return mass;
}

static void test_quarter_native_spring_adds_bounded_water_per_tick(void) {
    char directory[] = "build/ui_quarter_water_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){2, 0},
                                    chunk, err, sizeof(err)));
    uint64_t before = source_mass(chunk);
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){2, 0},
                                    chunk, err, sizeof(err)));
    uint64_t after = source_mass(chunk);
    printf("quarter-native spring source gain %.2f cells\n",
           (double)(after - before) / DC_FLUID_FULL);
    ASSERT_TRUE(after >= before + 3u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(after <= before + 6u * (uint64_t)DC_FLUID_FULL);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static bool basin_motion(dc_level_view_t *view, double *speed,
                         uint64_t *high_mass, char *err, uint32_t cap) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    double total_speed = 0.0;
    uint32_t wet_cells = 0u;
    *high_mass = 0u;
    for (int64_t x = 1; x <= 3; ++x) {
        if (!dc_level_view_chunk(view, (dc_chunk_coord_t){x, 0},
                                 chunk, err, cap)) { free(chunk); return false; }
        for (uint32_t y = 0u; y < 54u; ++y)
            for (uint32_t cx = 0u; cx < DC_CHUNK_SIDE; ++cx) {
                uint32_t index = y * DC_CHUNK_SIDE + cx;
                uint32_t mass = chunk->cells[index].fluid_mass;
                if (y < 20u) *high_mass += mass;
                if (mass >= DC_FLUID_FULL / 2u) {
                    total_speed += fabsf(chunk->face_velocity[index].y);
                    ++wet_cells;
                }
            }
    }
    free(chunk);
    *speed = wet_cells ? total_speed / wet_cells : 0.0;
    return wet_cells > 100u;
}

static void test_quarter_native_basin_settles_after_spring_stops(void) {
    char directory[] = "build/ui_quarter_settle_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    for (uint32_t tick = 0u; tick < 60u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    double early_speed = 0.0, steady_speed = 0.0, late_speed = 0.0;
    uint64_t early_high = 0u, steady_high = 0u, late_high = 0u;
    ASSERT_TRUE(basin_motion(view, &early_speed, &early_high, err, sizeof(err)));
    for (uint32_t tick = 0u; tick < 180u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(basin_motion(view, &steady_speed, &steady_high, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    for (uint32_t tick = 0u; tick < 120u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(basin_motion(view, &late_speed, &late_high, err, sizeof(err)));
    printf("quarter-native basin mean |vy| %.3f -> %.3f -> %.3f, high water %.2f -> %.2f -> %.2f cells\n",
           early_speed, steady_speed, late_speed,
           (double)early_high / DC_FLUID_FULL,
           (double)steady_high / DC_FLUID_FULL,
           (double)late_high / DC_FLUID_FULL);
    ASSERT_TRUE(steady_speed < 0.5);
    ASSERT_TRUE(steady_high < 30u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(late_speed < steady_speed * 0.5);
    ASSERT_TRUE(late_high < 2u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

int main(void) {
    RUN(test_quarter_native_spring_adds_bounded_water_per_tick);
    RUN(test_quarter_native_basin_settles_after_spring_stops);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
