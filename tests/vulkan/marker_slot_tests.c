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

static void test_marker_pool_in_slot_above_sixty_four_stays_bounded(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (uint32_t x = 8; x < 56; ++x)
        chunk->cells[36u * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    for (uint32_t y = 24; y < 36; ++y)
        for (uint32_t x = 8; x < 56; ++x)
            chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64,
                              "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 64u, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 64u, err, sizeof(err)));
    uint32_t first_count = 0, final_count = 0;
    for (uint32_t i = 0; i < 80u; ++i) {
        ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_marker_count(gpu, 64u, &final_count));
        if (i == 0u) first_count = final_count;
    }
    printf("slot 64 marker count first=%u final=%u\n", first_count, final_count);
    ASSERT_TRUE(first_count > 0u);
    ASSERT_TRUE(final_count <= DC_MARKERS_PER_CHUNK);
    ASSERT_TRUE(final_count < first_count * 2u);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 64u, chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->marker_count <= DC_MARKERS_PER_CHUNK);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}

int main(void) {
    RUN(test_marker_pool_in_slot_above_sixty_four_stays_bounded);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
