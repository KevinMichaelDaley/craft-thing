#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

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
        count += chunk->cells[i].material == DC_MATERIAL_GAS;
    return count;
}

static void test_gas_rises_through_air_on_gpu(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[40u * 64u + 10u].material = DC_MATERIAL_GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 8u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_EQ(gas_count(left), 1u);
    ASSERT_EQ(left->cells[32u * 64u + 10u].material, DC_MATERIAL_GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_timed_gas_rises_at_world_tick_rate(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[40u * 64u + 10u].material = DC_MATERIAL_GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_tick_seconds(gpu, 1.0f / 30.0f));
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_EQ(gas_count(left), 1u);
    ASSERT_EQ(left->cells[38u * 64u + 10u].material, DC_MATERIAL_GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_fractional_gas_steps_accumulate_without_extra_motion(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[40u * 64u + 10u].material = DC_MATERIAL_GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_tick_seconds(gpu, 1.0f / 120.0f));
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_EQ(left->cells[40u * 64u + 10u].material, DC_MATERIAL_GAS);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_EQ(gas_count(left), 1u);
    ASSERT_EQ(left->cells[39u * 64u + 10u].material, DC_MATERIAL_GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_gas_crosses_chunk_edge_around_lid(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[30u * 64u + 63u].material = DC_MATERIAL_GAS;
    left->cells[29u * 64u + 63u].material = DC_MATERIAL_STONE;
    left->cells[29u * 64u + 62u].material = DC_MATERIAL_STONE;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_tick_steps(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0u, left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1u, right, err, sizeof(err)));
    ASSERT_EQ(gas_count(left) + gas_count(right), 1u);
    ASSERT_EQ(right->cells[29u * 64u].material, DC_MATERIAL_GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_competing_gas_moves_have_one_winner(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[30u * 64u + 61u].material = DC_MATERIAL_GAS;
    left->cells[30u * 64u + 63u].material = DC_MATERIAL_GAS;
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
    ASSERT_EQ(left->cells[29u * 64u + 62u].material, DC_MATERIAL_GAS);
    ASSERT_EQ(left->cells[30u * 64u + 61u].material, DC_MATERIAL_AIR);
    ASSERT_EQ(left->cells[30u * 64u + 63u].material, DC_MATERIAL_GAS);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_lateral_preference_rotates_without_losing_gas(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    dc_chunk_t *saved = calloc(1, sizeof(*saved));
    ASSERT_TRUE(left && right && saved);
    left->cells[30u * 64u + 10u].material = DC_MATERIAL_GAS;
    for (uint32_t x = 9u; x <= 11u; ++x)
        left->cells[29u * 64u + x].material = DC_MATERIAL_STONE;
    dc_gpu_t *even = grid(left, right, err, sizeof(err));
    dc_gpu_t *odd = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(even && odd);
    odd->gas_tick = 1u;
    ASSERT_TRUE(dc_gpu_tick_steps(even, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_steps(odd, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(even, 0u, saved, err, sizeof(err)));
    ASSERT_EQ(saved->cells[30u * 64u + 9u].material, DC_MATERIAL_GAS);
    ASSERT_EQ(gas_count(saved), 1u);
    ASSERT_TRUE(dc_gpu_download_chunk(odd, 0u, saved, err, sizeof(err)));
    ASSERT_EQ(saved->cells[30u * 64u + 11u].material, DC_MATERIAL_GAS);
    ASSERT_EQ(gas_count(saved), 1u);
    dc_gpu_destroy(even); dc_gpu_destroy(odd);
    free(left); free(right); free(saved);
    PASS();
}

static void test_gas_at_chunk_edge_requests_streamed_neighbor(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[20u * 64u + 63u].material = DC_MATERIAL_GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    uint32_t masks[DC_GPU_CHUNK_SLOTS] = {0};
    ASSERT_TRUE(dc_gpu_wet_edge_masks(gpu, masks, DC_GPU_CHUNK_SLOTS,
                                       err, sizeof(err)));
    ASSERT_TRUE((masks[0] & 1u) != 0u);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

static void test_gas_pass_has_bounded_gpu_cost(void) {
    char err[256] = {0};
    dc_chunk_t *left = calloc(1, sizeof(*left));
    dc_chunk_t *right = calloc(1, sizeof(*right));
    ASSERT_TRUE(left && right);
    left->cells[40u * 64u + 10u].material = DC_MATERIAL_GAS;
    dc_gpu_t *gpu = grid(left, right, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    uint64_t idle_min = UINT64_MAX, active_min = UINT64_MAX;
    for (uint32_t phase = 0u; phase < 2u; ++phase) {
        gpu->gas_active = phase != 0u;
        for (uint32_t repeat = 0u; repeat < 5u; ++repeat) {
            dc_gpu_tick_capture_t capture = {0};
            ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
            uint64_t *best = phase ? &active_min : &idle_min;
            if (capture.stages[2].gpu_ns < *best)
                *best = capture.stages[2].gpu_ns;
        }
    }
    printf("gas GPU granular stage: idle %.3f ms, active %.3f ms\n",
           idle_min / 1e6, active_min / 1e6);
    ASSERT_TRUE(active_min < idle_min + 3000000u);
    dc_gpu_destroy(gpu);
    free(left); free(right);
    PASS();
}

int main(void) {
    RUN(test_gas_rises_through_air_on_gpu);
    RUN(test_timed_gas_rises_at_world_tick_rate);
    RUN(test_fractional_gas_steps_accumulate_without_extra_motion);
    RUN(test_gas_crosses_chunk_edge_around_lid);
    RUN(test_competing_gas_moves_have_one_winner);
    RUN(test_lateral_preference_rotates_without_losing_gas);
    RUN(test_gas_at_chunk_edge_requests_streamed_neighbor);
    RUN(test_gas_pass_has_bounded_gpu_cost);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
