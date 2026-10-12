#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/structural/gpu.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e);                \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_isolated_stone_automatically_becomes_a_falling_body(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 24; y++)
        for (unsigned x = 20; x < 24; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body = {0};
    bool extracted =
        dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err));
    dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err));
    bool consumed = chunk->cells[20 * 64 + 20].material == DC_MATERIAL_AIR;
    dc_gpu_destroy(gpu);
    free(chunk);
    ASSERT_TRUE(extracted);
    ASSERT_TRUE(consumed);
    ASSERT_TRUE(body.y_fp > 20 * 65536);
    PASS();
}
static void test_component_touching_cold_boundary_remains_terrain(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 24; y++)
        for (unsigned x = 0; x < 4; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    bool retained = chunk->cells[20 * 64].material == DC_MATERIAL_STONE;
    dc_gpu_destroy(gpu);
    free(chunk);
    ASSERT_TRUE(retained);
    PASS();
}
static void test_removing_last_support_triggers_automatic_fall(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 24; y++)
        for (unsigned x = 20; x < 24; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    for (unsigned y = 24; y < 64; y++)
        chunk->cells[y * 64 + 20].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    bool supported = chunk->cells[20 * 64 + 20].material == DC_MATERIAL_STONE;
    chunk->cells[24 * 64 + 20].material = DC_MATERIAL_AIR;
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body = {0};
    bool extracted =
        dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err));
    dc_gpu_destroy(gpu);
    free(chunk);
    ASSERT_TRUE(supported);
    ASSERT_TRUE(extracted);
    PASS();
}
int main(void) {
    RUN(test_isolated_stone_automatically_becomes_a_falling_body);
    RUN(test_component_touching_cold_boundary_remains_terrain);
    RUN(test_removing_last_support_triggers_automatic_fall);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
