#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static double seam_divergence(const dc_chunk_t *left,
                              const dc_chunk_t *right, uint32_t y) {
    uint32_t edge = y * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u;
    uint32_t next = y * DC_CHUNK_SIDE;
    float crossing = left->face_velocity[edge].x;
    float left_divergence = crossing - left->face_velocity[edge - 1u].x +
        left->face_velocity[edge].y -
        left->face_velocity[edge - DC_CHUNK_SIDE].y;
    float right_divergence = right->face_velocity[next].x - crossing +
        right->face_velocity[next].y -
        right->face_velocity[next - DC_CHUNK_SIDE].y;
    return fabsf(left_divergence) + fabsf(right_divergence);
}

static double column_level(const dc_chunk_t *chunk, uint32_t x) {
    double level = 0.0;
    for (uint32_t y = 0u; y < 63u; ++y)
        level += (double)chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass /
                 DC_FLUID_FULL;
    return level;
}

static void test_three_workspace_deep_basin_pressure(void) {
    char err[256] = {0};
    dc_gpu_t *split[3] = {0}, *monolithic = NULL;
    dc_chunk_t *initial[3] = {0}, *split_result[3] = {0};
    dc_chunk_t *mono_result[3] = {0};
    uint64_t expected_mass = 0u;
    for (uint32_t part = 0u; part < 3u; ++part) {
        initial[part] = calloc(1, sizeof(dc_chunk_t));
        split_result[part] = calloc(1, sizeof(dc_chunk_t));
        mono_result[part] = calloc(1, sizeof(dc_chunk_t));
        ASSERT_TRUE(initial[part] && split_result[part] && mono_result[part]);
        initial[part]->coord.x = (int64_t)part;
        for (uint32_t x = 0u; x < DC_CHUNK_SIDE; ++x)
            initial[part]->cells[63u * DC_CHUNK_SIDE + x].material =
                DC_MATERIAL_STONE;
        for (uint32_t y = 18u + part * 6u; y < 63u; ++y)
            for (uint32_t x = 0u; x < DC_CHUNK_SIDE; ++x) {
                initial[part]->cells[y * DC_CHUNK_SIDE + x].fluid_mass =
                    DC_FLUID_FULL;
                expected_mass += DC_FLUID_FULL;
            }
    }
    ASSERT_TRUE(dc_gpu_create(&split[0], 64u, 64u,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    for (uint32_t part = 1u; part < 3u; ++part)
        ASSERT_TRUE(dc_gpu_create_shared(&split[part], split[0], 64u, 64u,
            "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_create(&monolithic, 192u, 64u,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    for (uint32_t part = 0u; part < 3u; ++part) {
        ASSERT_TRUE(dc_gpu_upload_chunk(split[part], 0u, initial[part],
                                         err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(split[part], 0u, 0u, 0u,
                                    err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_upload_chunk(monolithic, part, initial[part],
                                         err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(monolithic, part, 0u, part,
                                    err, sizeof(err)));
    }
    dc_gpu_boundary_t boundary = { .main_slot = 0u, .other_slot = 0u,
        .main_x = 0u, .main_y = 0u, .other_x = 0u, .other_y = 0u,
        .other_side = 1u };
    for (uint32_t tick = 0u; tick < 120u; ++tick) {
        for (uint32_t part = 0u; part < 3u; ++part)
            ASSERT_TRUE(dc_gpu_tick_step(split[part], err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_tick_step(monolithic, err, sizeof(err)));
        for (uint32_t seam = 0u; seam < 2u; ++seam)
            ASSERT_TRUE(dc_gpu_boundary_exchange(split[seam], split[seam + 1u],
                &boundary, 1u, 1.0f, err, sizeof(err)));
    }
    uint64_t split_mass = 0u, mono_mass = 0u;
    for (uint32_t part = 0u; part < 3u; ++part) {
        ASSERT_TRUE(dc_gpu_download_chunk(split[part], 0u, split_result[part],
                                           err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(monolithic, part, mono_result[part],
                                           err, sizeof(err)));
        for (uint32_t i = 0u; i < DC_CHUNK_CELLS; ++i) {
            split_mass += split_result[part]->cells[i].fluid_mass;
            mono_mass += mono_result[part]->cells[i].fluid_mass;
        }
    }
    ASSERT_EQ(split_mass, expected_mass);
    ASSERT_EQ(mono_mass, expected_mass);
    double split_divergence = 0.0, mono_divergence = 0.0;
    double split_level_error = 0.0, mono_level_error = 0.0;
    for (uint32_t seam = 0u; seam < 2u; ++seam) {
        double split_left = column_level(split_result[seam], 63u);
        double split_right = column_level(split_result[seam + 1u], 0u);
        double mono_left = column_level(mono_result[seam], 63u);
        double mono_right = column_level(mono_result[seam + 1u], 0u);
        split_level_error += fabs(split_left - split_right);
        mono_level_error += fabs(mono_left - mono_right);
        for (uint32_t y = 25u; y < 63u; ++y) {
            split_divergence += seam_divergence(split_result[seam],
                                                split_result[seam + 1u], y);
            mono_divergence += seam_divergence(mono_result[seam],
                                               mono_result[seam + 1u], y);
        }
    }
    dc_gpu_tick_capture_t capture = {0};
    ASSERT_TRUE(dc_gpu_tick_capture(monolithic, &capture, err, sizeof(err)));
    printf("three-workspace basin divergence split %.3f mono %.3f, "
           "level error split %.3f mono %.3f, fluid GPU %.3f ms\n",
           split_divergence, mono_divergence, split_level_error,
           mono_level_error, capture.stages[1].gpu_ns / 1e6);
    ASSERT_TRUE(split_divergence <= mono_divergence * 1.5 + 0.2);
    ASSERT_TRUE(split_level_error <= mono_level_error + 1.0);
    ASSERT_TRUE(capture.stages[1].gpu_ns < 20000000u);
    for (uint32_t part = 0u; part < 3u; ++part) {
        free(initial[part]);
        free(split_result[part]);
        free(mono_result[part]);
    }
    for (uint32_t part = 3u; part > 0u; --part)
        dc_gpu_destroy(split[part - 1u]);
    dc_gpu_destroy(monolithic);
    PASS();
}

int main(void) {
    RUN(test_three_workspace_deep_basin_pressure);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
