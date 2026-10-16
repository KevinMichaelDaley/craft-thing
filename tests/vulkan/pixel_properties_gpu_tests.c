#define main pixel_device_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

#include <math.h>
#include "../../tools/diagnostics/shader_fixture.h"

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

typedef struct {
    probe_t probe;
    dc_gpu_t gpu;
    VkBuffer body;
    VkDeviceMemory body_memory;
    uint32_t *shape;
} property_fixture_t;

static void fixture_start(property_fixture_t *f, unsigned tip) {
    probe_t *p = &f->probe;
    char err[256];
    if (!initialize(p))
        exit(2);
    f->gpu = (dc_gpu_t){.device = p->device,
                        .physical = p->physical,
                        .width = 64,
                        .height = 64,
                        .page_width = 1,
                        .page_height = 1};
    if (!dc_gpu_make_mapped_buffer(&f->gpu, dc_gpu_body_storage_bytes(&f->gpu),
                                   &f->body, &f->body_memory,
                                   &f->gpu.body_mapped, err, sizeof(err)) ||
        !dc_gpu_make_mapped_buffer(&f->gpu, 39u * sizeof(uint32_t), &p->buffer,
                                   &p->memory, (void **)&p->mapped, err,
                                   sizeof(err))) {
        printf("allocation failure: %s\n", err);
        exit(2);
    }
    dc_gpu_body_record_t *b = f->gpu.body_mapped;
    b->body = (dc_gpu_body_t){.id = 37, .active = 1, .width = 3, .height = 3};
    b->local_x_fp = 10 << 16;
    b->local_y_fp = 10 << 16;
    b->piece_count = DC_GPU_PIXEL_BODY_FLAG | 5u;
    b->compound_reserved =
        dc_gpu_pixel_pool_word_offset(&f->gpu) + DC_GPU_PIXEL_POOL_HEADER_WORDS;
    f->shape = (uint32_t *)f->gpu.body_mapped + b->compound_reserved;
    uint32_t header[10] = {42, 1, 1, 1, 0, 0, 3, 3, 9, 0};
    memcpy(f->shape, header, sizeof(header));
    f->shape[10] = 0x122;
    f->shape[12] = 0x100;
    f->shape[14] = tip << 8;
    p->mapped[0] = 1;
    const uint32_t binding = 4;
    if (!fixture_body_pipeline(p, &f->gpu, f->body,
                               "build/diagnostics/pixel_properties_probe.spv",
                               &binding, 1, UINT32_MAX))
        exit(2);
}
static void fixture_run(property_fixture_t *f, const char *stage) {
    probe_t *p = &f->probe;
    if (!begin(p))
        exit(2);
    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                                VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p->command, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier,
                         0, NULL, 0, NULL);
    vkCmdBindPipeline(p->command, VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline);
    vkCmdBindDescriptorSets(p->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            p->layout, 0, 1, &p->descriptor, 0, NULL);
    uint32_t push[8] = {64, 64, 2, 1, 0, 0, 0, 0};
    vkCmdPushConstants(p->command, p->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), push);
    vkCmdDispatch(p->command, 1, 1, 1);
    host_barrier(p, 1);
    if (!check(vkEndCommandBuffer(p->command), "property command end") ||
        !submit(p, stage, 1))
        exit(2);
}
static void fixture_stop(property_fixture_t *f) {
    probe_t *p = &f->probe;
    fixture_release_pipeline(p);
    vkUnmapMemory(p->device, p->memory);
    vkDestroyBuffer(p->device, p->buffer, NULL);
    vkFreeMemory(p->device, p->memory, NULL);
    vkUnmapMemory(p->device, f->body_memory);
    vkDestroyBuffer(p->device, f->body, NULL);
    vkFreeMemory(p->device, f->body_memory, NULL);
    vkDestroyFence(p->device, p->fence, NULL);
    vkDestroyCommandPool(p->device, p->pool, NULL);
    vkDestroyDevice(p->device, NULL);
    vkDestroyInstance(p->instance, NULL);
}
static void property_case(unsigned tip, double mass, double x, double y,
                          double inertia) {
    property_fixture_t f = {0};
    fixture_start(&f, tip);
    fixture_run(&f, "property reduction");
    dc_gpu_body_record_t b = *(dc_gpu_body_record_t *)f.gpu.body_mapped;
    fixture_stop(&f);
    printf("mass%.9g center%.9g,%.9g inertia%.9g\n", b.mass, b.center[0],
           b.center[1], b.inertia);
    ASSERT_TRUE(fabs(b.mass - mass) < 1e-5);
    ASSERT_TRUE(fabs(b.center[0] - x) < 1e-6);
    ASSERT_TRUE(fabs(b.center[1] - y) < 1e-6);
    ASSERT_TRUE(fabs(b.inertia - inertia) < 1e-5);
    ASSERT_EQ(b.motion_ready, 1u);
    PASS();
}
static void test_exact_mixed_concave_properties(void) {
    property_case(4, 7.8, 59. / 26., 15. / 13., 8.05);
}
static void test_mortar_uses_its_own_density(void) {
    property_case(3, 8.2, 18.7 / 8.2, 10. / 8.2, 191. / 3. - 449.69 / 8.2);
}
static void
test_cached_active_and_dormant_flags_refresh_without_geometry_reduction(void) {
    property_fixture_t f = {0};
    fixture_start(&f, 4);
    fixture_run(&f, "initial properties");
    dc_gpu_body_record_t *b = f.gpu.body_mapped;
    if (b->motion_ready != 1u) {
        fixture_stop(&f);
        ASSERT_TRUE(false);
    }
    float mass = b->mass, inertia = b->inertia,
          center[2] = {b->center[0], b->center[1]};
    uint32_t geometry[42];
    memcpy(geometry, f.shape, sizeof(geometry));
    const uint32_t bindings[] = {1, 2, 3, 4};
    if (!fixture_body_pipeline(&f.probe, &f.gpu, f.body,
                               "build/diagnostics/rigid_prepare_probe.spv",
                               bindings, 4, 0))
        exit(2);
    bool valid = true;
    char err[256];
    for (unsigned i = 0; i < 6; i++) {
        unsigned flags = i < 4 ? i : (i == 4 ? DC_GPU_BODY_KINEMATIC : 0);
        if (i == 4) {
            b->body.active = 0;
            b->reserved[0] = DC_GPU_STRUCTURAL_CONVERTED_TAG;
            b->reserved[1] = 1;
        }
        if (!dc_gpu_set_body_motion(&f.gpu, 37, .25f, .01f, flags, err,
                                    sizeof(err))) {
            printf("motion failure: %s\n", err);
            exit(2);
        }
        valid &= b->motion_ready == 1u;
        fixture_run(&f, i < 4 ? "active flag refresh" : "dormant flag refresh");
        valid &= memcmp(&mass, &b->mass, sizeof(mass)) == 0 &&
                 memcmp(&inertia, &b->inertia, sizeof(inertia)) == 0;
        valid &= memcmp(center, b->center, sizeof(center)) == 0 &&
                 memcmp(geometry, f.shape, sizeof(geometry)) == 0;
        valid &= fabs(b->inverse_mass - ((flags & 1) ? 0. : 1. / mass)) < 1e-6;
        valid &=
            fabs(b->inverse_inertia - ((flags & 3) ? 0. : 1. / inertia)) < 1e-6;
        printf("flags%u active%u inverse mass%.9g inertia%.9g valid%d\n", flags,
               b->body.active, b->inverse_mass, b->inverse_inertia, valid);
    }
    fixture_stop(&f);
    ASSERT_TRUE(valid);
    PASS();
}
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    RUN(test_exact_mixed_concave_properties);
    RUN(test_mortar_uses_its_own_density);
    RUN(test_cached_active_and_dormant_flags_refresh_without_geometry_reduction);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
