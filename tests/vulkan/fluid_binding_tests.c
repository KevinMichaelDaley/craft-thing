#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/fire/bulk_phase_gpu.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_fluid_step_keeps_descriptors_bound_across_pipeline_layouts(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (uint32_t y = 4u; y < 8u; y++)
        for (uint32_t x = 4u; x < 8u; x++) {
            uint32_t cell = y * 64u + x;
            chunk->cells[cell].material = DC_MATERIAL_WATER;
            chunk->cells[cell].fluid_mass = DC_FLUID_FULL;
        }
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    printf("validation errors after fluid steps: %u\n",
           dc_gpu_validation_errors(gpu));
    ASSERT_EQ(dc_gpu_validation_errors(gpu), 0u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}
static void test_bulk_phase_transition_keeps_descriptors_bound(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (uint32_t y = 4u; y < 8u; y++)
        for (uint32_t x = 4u; x < 8u; x++) {
            uint32_t cell = y * 64u + x;
            chunk->cells[cell].material = DC_MATERIAL_WATER;
            chunk->cells[cell].fluid_mass = DC_FLUID_FULL;
        }
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    fr_bulk_phase_chunk_t bulk, out;
    memset(&bulk, 0, sizeof(bulk));
    ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(gpu, 0, chunk, &bulk, err,
                                               sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_step(gpu, 1.f / 60.f, err, sizeof(err)));
    ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(gpu, 0, &out, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu, err, sizeof(err)));
    printf("validation errors after bulk-to-fluid transition: %u\n",
           dc_gpu_validation_errors(gpu));
    ASSERT_EQ(dc_gpu_validation_errors(gpu), 0u);
    dc_gpu_destroy(gpu);
    free(chunk);
    PASS();
}
int main(void) {
    printf("=== Fluid Binding Tests ===\n\n");
    RUN(test_fluid_step_keeps_descriptors_bound_across_pipeline_layouts);
    RUN(test_bulk_phase_transition_keeps_descriptors_bound);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
