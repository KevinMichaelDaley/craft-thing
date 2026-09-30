#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

enum { GAS = 6u };

static dc_gpu_t *grid(dc_chunk_t *left, dc_chunk_t *right,
                      char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 128u, 64u,
                       "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_upload_chunk(gpu, 0u, left, err, cap) ||
        !dc_gpu_upload_chunk(gpu, 1u, right, err, cap) ||
        !dc_gpu_set_page(gpu, 0u, 0u, 0u, err, cap) ||
        !dc_gpu_set_page(gpu, 1u, 0u, 1u, err, cap)) {
        dc_gpu_destroy(gpu);
        return NULL;
    }
    return gpu;
}

static uint32_t gas_count(const dc_chunk_t *chunk) {
    uint32_t count = 0u;
    for (uint32_t i = 0u; i < DC_CHUNK_CELLS; ++i)
        count += chunk->cells[i].material == GAS;
    return count;
}

static void test_gas_rises_through_air_on_gpu(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[40u * 64u + 10u].material = GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 8u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_EQ(gas_count(left), 1u);
    ASSERT_EQ(left->cells[32u * 64u + 10u].material, GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_gas_crosses_chunk_edge_around_lid(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[30u * 64u + 63u].material = GAS;
    left->cells[29u * 64u + 63u].material = DC_MATERIAL_STONE;
    left->cells[29u * 64u + 62u].material = DC_MATERIAL_STONE;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1u, right, err, sizeof(err)));
    ASSERT_EQ(gas_count(left) + gas_count(right), 1u);
    ASSERT_EQ(right->cells[29u * 64u].material, GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_competing_gas_moves_have_one_winner(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[30u * 64u + 61u].material = GAS;
    left->cells[30u * 64u + 63u].material = GAS;
    for (uint32_t x = 60u; x <= 64u; ++x) {
        if (x == 62u) continue;
        dc_chunk_t *chunk = x < 64u ? left : right;
        chunk->cells[29u * 64u + x % 64u].material = DC_MATERIAL_STONE;
    }
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1u, right, err, sizeof(err)));
    ASSERT_EQ(gas_count(left) + gas_count(right), 2u);
    ASSERT_EQ(left->cells[29u * 64u + 62u].material, GAS);
    ASSERT_EQ(left->cells[30u * 64u + 61u].material, DC_MATERIAL_AIR);
    ASSERT_EQ(left->cells[30u * 64u + 63u].material, GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

int main(void) {
    RUN(test_gas_rises_through_air_on_gpu);
    RUN(test_gas_crosses_chunk_edge_around_lid);
    RUN(test_competing_gas_moves_have_one_winner);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
