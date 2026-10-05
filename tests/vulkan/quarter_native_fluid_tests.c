#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/app/view_config.h"
#include "../../src/vulkan/gpu_internal.h"
#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *make_basin(float seconds, char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t x = i % DC_CHUNK_SIDE, y = i / DC_CHUNK_SIDE;
        if (x == 0 || x == DC_CHUNK_SIDE - 1u || y == DC_CHUNK_SIDE - 1u)
            chunk->cells[i].material = DC_MATERIAL_STONE;
        else if (x >= 20u && x < 44u && y == 8u)
            chunk->cells[i].fluid_mass = DC_FLUID_FULL;
    }
    bool okay = dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, cap) &&
                dc_gpu_upload_chunk(gpu, 0, chunk, err, cap) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, cap) &&
                dc_gpu_set_fluid_interval(gpu, FLUID_INTERVAL) &&
                dc_gpu_set_tick_seconds(gpu, seconds);
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static void test_quarter_native_two_tick_fluid_preserves_volume(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_basin(1.0f / 60.0f, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t i = 0; i < 120u; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    ASSERT_EQ(gpu->fluid_tick, 60u);
    ASSERT_EQ(gpu->fluid_phase, 0u);
    ASSERT_EQ(gpu->fluid_step_scale, 2.0f);
    dc_chunk_t *saved = calloc(1, sizeof(*saved));
    ASSERT_TRUE(saved != NULL);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, saved, err, sizeof(err)));
    uint64_t mass = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        ASSERT_TRUE(saved->cells[i].fluid_mass <= DC_FLUID_FULL);
        mass += saved->cells[i].fluid_mass;
    }
    ASSERT_EQ(mass, 24u * (uint64_t)DC_FLUID_FULL);
    dc_gpu_destroy(gpu);
    free(saved);
    PASS();
}

static void test_quarter_native_equal_elapsed_time_matches_frame_cadences(void) {
    char err[256] = {0};
    dc_gpu_t *fast = make_basin(1.0f / 60.0f, err, sizeof(err));
    dc_gpu_t *slow = make_basin(2.0f / 60.0f, err, sizeof(err));
    ASSERT_TRUE(fast && slow);
    for (uint32_t i = 0; i < 60u; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(fast, err, sizeof(err)));
    for (uint32_t i = 0; i < 30u; ++i)
        ASSERT_TRUE(dc_gpu_tick_step(slow, err, sizeof(err)));
    ASSERT_EQ(fast->fluid_tick, 30u);
    ASSERT_EQ(slow->fluid_tick, 30u);
    dc_chunk_t *a = calloc(1, sizeof(*a)), *b = calloc(1, sizeof(*b));
    ASSERT_TRUE(a && b);
    ASSERT_TRUE(dc_gpu_download_chunk(fast, 0, a, err, sizeof(err)) &&
                dc_gpu_download_chunk(slow, 0, b, err, sizeof(err)));
    ASSERT_EQ(memcmp(a->cells, b->cells, sizeof(a->cells)), 0);
    ASSERT_EQ(memcmp(a->face_velocity, b->face_velocity, sizeof(a->face_velocity)), 0);
    dc_gpu_destroy(fast);
    dc_gpu_destroy(slow);
    free(a);
    free(b);
    PASS();
}

int main(void) {
    RUN(test_quarter_native_two_tick_fluid_preserves_volume);
    RUN(test_quarter_native_equal_elapsed_time_matches_frame_cadences);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
