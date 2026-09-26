#include <stdint.h>
#include <stdio.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *make_grid(dc_chunk_t *left, dc_chunk_t *right,
                            char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_upload_chunk(gpu, 0, left, err, cap) ||
        !dc_gpu_upload_chunk(gpu, 1, right, err, cap) ||
        !dc_gpu_set_page(gpu, 0, 0, 0, err, cap) ||
        !dc_gpu_set_page(gpu, 1, 0, 1, err, cap)) {
        dc_gpu_destroy(gpu);
        return NULL;
    }
    return gpu;
}

static void test_water_falls_and_crosses_resident_chunk_edge(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    left.cells[2 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    ASSERT_TRUE(saved_left.cells[3 * 64 + 63].fluid_mass > 0);
    uint64_t total = 0, right_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        total += saved_left.cells[i].fluid_mass + saved_right.cells[i].fluid_mass;
        right_mass += saved_right.cells[i].fluid_mass;
    }
    ASSERT_EQ(total, (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(right_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_closed_basin_conserves_mass_for_long_run(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0}, saved_right = {0};
    for (uint32_t y = 10; y <= 20; ++y) {
        left.cells[y * 64 + 60].material = DC_MATERIAL_STONE;
        right.cells[y * 64 + 4].material = DC_MATERIAL_STONE;
    }
    for (uint32_t x = 60; x < 64; ++x) {
        left.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    for (uint32_t x = 0; x <= 4; ++x) {
        right.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    left.cells[11 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    left.cells[11 * 64 + 62].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t i = 0; i < 100; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved_right, err, sizeof(err)));
    uint64_t total = 0, right_mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        total += saved_left.cells[i].fluid_mass + saved_right.cells[i].fluid_mass;
        right_mass += saved_right.cells[i].fluid_mass;
        if (saved_left.cells[i].material == DC_MATERIAL_STONE)
            ASSERT_EQ(saved_left.cells[i].fluid_mass, 0u);
        if (saved_right.cells[i].material == DC_MATERIAL_STONE)
            ASSERT_EQ(saved_right.cells[i].fluid_mass, 0u);
    }
    ASSERT_EQ(total, (uint64_t)2 * DC_FLUID_FULL);
    ASSERT_TRUE(right_mass > 0);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_unloaded_neighbor_keeps_mass_in_source(void) {
    char err[256] = {0};
    dc_chunk_t left = {0}, right = {0}, saved_left = {0};
    left.cells[63 * 64 + 63].fluid_mass = DC_FLUID_FULL;
    dc_gpu_t *gpu = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    for (uint32_t i = 0; i < 8; ++i)
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved_left, err, sizeof(err)));
    uint64_t total = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) total += saved_left.cells[i].fluid_mass;
    ASSERT_EQ(total, (uint64_t)DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_water_falls_and_crosses_resident_chunk_edge);
    RUN(test_closed_basin_conserves_mass_for_long_run);
    RUN(test_unloaded_neighbor_keeps_mass_in_source);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
