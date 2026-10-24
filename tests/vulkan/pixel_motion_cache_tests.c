#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

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

static void test_motion_updates_preserve_ready_pixel_geometry_properties(void) {
    dc_gpu_body_record_t records[DC_GPU_BODY_CAPACITY] = {0};
    records[0].body =
        (dc_gpu_body_t){.id = 37, .active = 1, .width = 300, .height = 400};
    records[0].piece_count = DC_GPU_PIXEL_BODY_FLAG | 100000u;
    records[0].compound_reserved = 1000000u;
    records[0].motion_ready = 1;
    records[0].mass = 270000;
    records[0].inertia = 1234567;
    records[0].density = 2.7f;
    records[0].center[0] = 149.5f;
    records[0].center[1] = 200.25f;
    dc_gpu_t gpu = {.body_mapped = records};
    char err[128] = {0};
    ASSERT_TRUE(dc_gpu_set_body_motion(
        &gpu, 37, .5f, .01f, DC_GPU_BODY_KINEMATIC, err, sizeof(err)));
    ASSERT_TRUE(records[0].motion_ready != 0u);
    ASSERT_EQ(records[0].mass, 270000);
    ASSERT_EQ(records[0].inertia, 1234567);
    ASSERT_EQ(records[0].center[0], 149.5f);
    ASSERT_EQ(records[0].center[1], 200.25f);
    ASSERT_EQ(records[0].compound_reserved, 1000000u);
    ASSERT_TRUE(dc_gpu_set_body_motion(
        &gpu, 37, -.5f, -.01f, DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err)));
    ASSERT_TRUE(records[0].motion_ready != 0u);
    ASSERT_EQ(records[0].motion_flags, DC_GPU_BODY_LOCK_ROTATION);
    PASS();
}
static void test_uncached_geometry_still_requests_initial_properties(void) {
    dc_gpu_body_record_t records[DC_GPU_BODY_CAPACITY] = {0};
    records[0].body =
        (dc_gpu_body_t){.id = 37, .active = 1, .width = 300, .height = 400};
    records[0].piece_count = DC_GPU_PIXEL_BODY_FLAG | 100000u;
    dc_gpu_t gpu = {.body_mapped = records};
    char err[128] = {0};
    ASSERT_TRUE(
        dc_gpu_set_body_motion(&gpu, 37, .5f, .01f, 0, err, sizeof(err)));
    ASSERT_EQ(records[0].motion_ready, 0u);
    PASS();
}
static void test_legacy_shapes_keep_initialization_contract(void) {
    dc_gpu_body_record_t records[DC_GPU_BODY_CAPACITY] = {0};
    records[0].body =
        (dc_gpu_body_t){.id = 37, .active = 1, .width = 3, .height = 4};
    records[0].motion_ready = 1;
    dc_gpu_t gpu = {.body_mapped = records};
    char err[128] = {0};
    ASSERT_TRUE(
        dc_gpu_set_body_motion(&gpu, 37, .5f, .01f, 0, err, sizeof(err)));
    ASSERT_EQ(records[0].motion_ready, 0u);
    PASS();
}
static void test_motion_lookup_does_not_rebuild_registry(void) {
    dc_gpu_body_record_t records[DC_GPU_BODY_CAPACITY] = {0};
    uint32_t ids[DC_GPU_BODY_CAPACITY];
    for (unsigned i = 0; i < DC_GPU_BODY_CAPACITY; ++i) ids[i] = 12345u;
    records[DC_GPU_BODY_CAPACITY-1u].body =
        (dc_gpu_body_t){.id = 37, .active = 1, .width = 3, .height = 4};
    dc_gpu_t gpu = {.body_mapped = records, .body_ids = ids, .body_count = 19};
    char err[128] = {0};
    ASSERT_TRUE(dc_gpu_set_body_motion(&gpu, 37, .5f, .01f, 0, err, sizeof(err)));
    ASSERT_EQ(gpu.body_count, 19u);
    for (unsigned i = 0; i < DC_GPU_BODY_CAPACITY; ++i) ASSERT_EQ(ids[i], 12345u);
    PASS();
}
int main(void) {
    RUN(test_motion_lookup_does_not_rebuild_registry);
    RUN(test_motion_updates_preserve_ready_pixel_geometry_properties);
    RUN(test_uncached_geometry_still_requests_initial_properties);
    RUN(test_legacy_shapes_keep_initialization_contract);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
