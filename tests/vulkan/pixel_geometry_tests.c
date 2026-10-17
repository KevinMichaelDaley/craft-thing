#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static VkDeviceSize label_ranges[2];
void __real_vkUpdateDescriptorSets(VkDevice device, uint32_t write_count,
                                   const VkWriteDescriptorSet *writes,
                                   uint32_t copy_count,
                                   const VkCopyDescriptorSet *copies);
void __wrap_vkUpdateDescriptorSets(VkDevice device, uint32_t write_count,
                                   const VkWriteDescriptorSet *writes,
                                   uint32_t copy_count,
                                   const VkCopyDescriptorSet *copies) {
    for (unsigned i = 0; i < write_count; i++)
        if (writes[i].dstBinding == 28 || writes[i].dstBinding == 29)
            label_ranges[writes[i].dstBinding - 28] =
                writes[i].pBufferInfo->range;
    __real_vkUpdateDescriptorSets(device, write_count, writes, copy_count,
                                  copies);
}
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

static uint32_t *fixture(dc_gpu_body_record_t *b, uint32_t *words) {
    *words = 18;
    uint32_t *dense = calloc(*words, sizeof(*dense));
    if (!dense)
        return NULL;
    b->body.active = 1;
    b->body.id = 1;
    b->body.width = 10;
    b->body.height = 14;
    b->piece_count = DC_GPU_PIXEL_BODY_FLAG | 77u;
    for (unsigned y = 0; y < 14; y++)
        for (unsigned x = 0; x < 10; x++)
            if (x == 0 || y % 2 == 0) {
                unsigned p = y * 10 + x;
                dense[p / 8] |= (x == 0 && y == 0 ? 2u : 1u) << ((p % 8) * 4);
            }
    return dense;
}

static void
test_version_four_upgrade_preserves_materials_and_empty_cells(void) {
    dc_gpu_t gpu = {.width = 64, .height = 64};
    dc_gpu_body_record_t bodies[64] = {0};
    uint32_t words;
    uint32_t *pixels = fixture(bodies, &words);
    ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_upgrade_dense_pixels(&gpu, bodies, &pixels, &words));
    float density;
    ASSERT_TRUE(dc_gpu_pixel_geometry_valid(bodies, pixels, words, &density));
    ASSERT_TRUE(fabs(density - (76 * 2.7 + .6) / 77) < 1e-6);
    ASSERT_EQ(words, 42u);
    for (unsigned y = 0; y < 14; y++)
        for (unsigned x = 0; x < 10; x++) {
            unsigned p = y * 16 + x;
            uint32_t actual =
                (pixels[pixels[8] + 1 + p / 8] >> ((p % 8) * 4)) & 15u;
            uint32_t expected = x == 0 && y == 0       ? 2u
                                : x == 0 || y % 2 == 0 ? 1u
                                                       : 0u;
            ASSERT_EQ(actual, expected);
        }
    free(pixels);
    PASS();
}

static void test_corrupt_tiled_geometry_is_rejected(void) {
    dc_gpu_t gpu = {.width = 64, .height = 64};
    dc_gpu_body_record_t bodies[64] = {0};
    uint32_t words;
    uint32_t *pixels = fixture(bodies, &words);
    ASSERT_TRUE(pixels);
    ASSERT_TRUE(dc_gpu_upgrade_dense_pixels(&gpu, bodies, &pixels, &words));
    float density;
    ASSERT_TRUE(
        !dc_gpu_pixel_geometry_valid(bodies, pixels, words - 1, &density));
    pixels[8]++;
    ASSERT_TRUE(!dc_gpu_pixel_geometry_valid(bodies, pixels, words, &density));
    pixels[8]--;
    uint32_t original = pixels[10];
    pixels[10] = (original & ~15u) | 6u;
    ASSERT_TRUE(!dc_gpu_pixel_geometry_valid(bodies, pixels, words, &density));
    pixels[10] = original;
    bodies[0].piece_count--;
    ASSERT_TRUE(!dc_gpu_pixel_geometry_valid(bodies, pixels, words, &density));
    free(pixels);
    PASS();
}

static void test_truncated_dense_upgrade_keeps_input_allocation(void) {
    dc_gpu_t gpu = {.width = 64, .height = 64};
    dc_gpu_body_record_t bodies[64] = {0};
    uint32_t words;
    uint32_t *pixels = fixture(bodies, &words), *original = pixels;
    ASSERT_TRUE(pixels);
    words--;
    ASSERT_TRUE(!dc_gpu_upgrade_dense_pixels(&gpu, bodies, &pixels, &words));
    ASSERT_TRUE(pixels == original);
    ASSERT_EQ(bodies[0].compound_reserved, 0u);
    free(pixels);
    PASS();
}
static void test_empty_tile_cannot_alias_an_occupied_tile(void) {
    dc_gpu_body_record_t body = {.body = {.width = 32, .height = 1},
                                 .piece_count = DC_GPU_PIXEL_BODY_FLAG | 1u};
    uint32_t geometry[43] = {43, 1, 2, 1, 0, 0, 1, 1, 10, 0, 0, 1};
    float density;
    ASSERT_TRUE(dc_gpu_pixel_geometry_valid(&body, geometry, 43, &density));
    geometry[9] = geometry[8];
    ASSERT_TRUE(!dc_gpu_pixel_geometry_valid(&body, geometry, 43, &density));
    PASS();
}
static void
test_fracture_descriptors_cover_the_full_preallocated_tile_scratch(void) {
    dc_gpu_t *gpu = NULL;
    char err[256] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 67, 67, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    VkDeviceSize required =
        (VkDeviceSize)dc_gpu_pixel_pool_capacity(gpu) * 8 * sizeof(uint32_t);
    ASSERT_TRUE(label_ranges[0] >= required);
    ASSERT_TRUE(label_ranges[1] >= required);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_dense_fragment_upgrade_records_actual_occupied_bounds(void) {
    dc_gpu_t gpu = {.width = 64, .height = 64};
    dc_gpu_body_record_t bodies[64] = {0};
    bodies[0].body.active = 1;
    bodies[0].body.width = 40;
    bodies[0].body.height = 30;
    bodies[0].piece_count = DC_GPU_PIXEL_BODY_FLAG | 60u;
    uint32_t words = 150, *pixels = calloc(words, sizeof(*pixels));
    ASSERT_TRUE(pixels);
    for (unsigned y = 0; y < 15; y++)
        for (unsigned x = 36; x < 40; x++) {
            unsigned p = y * 40 + x;
            pixels[p / 8] |= 1u << ((p % 8) * 4);
        }
    ASSERT_TRUE(dc_gpu_upgrade_dense_pixels(&gpu, bodies, &pixels, &words));
    ASSERT_EQ(pixels[4], 36u);
    ASSERT_EQ(pixels[5], 0u);
    ASSERT_EQ(pixels[6], 40u);
    ASSERT_EQ(pixels[7], 15u);
    float density;
    ASSERT_TRUE(dc_gpu_pixel_geometry_valid(bodies, pixels, words, &density));
    free(pixels);
    PASS();
}

int main(void) {
    RUN(test_version_four_upgrade_preserves_materials_and_empty_cells);
    RUN(test_corrupt_tiled_geometry_is_rejected);
    RUN(test_truncated_dense_upgrade_keeps_input_allocation);
    RUN(test_empty_tile_cannot_alias_an_occupied_tile);
    RUN(test_fracture_descriptors_cover_the_full_preallocated_tile_scratch);
    RUN(test_dense_fragment_upgrade_records_actual_occupied_bounds);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
