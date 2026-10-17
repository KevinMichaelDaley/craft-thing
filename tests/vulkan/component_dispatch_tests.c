#define main device_probe_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

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

#include "../../tools/diagnostics/shader_fixture.h"

static bool initializes_owned_triplets(bool dirty) {
    probe_t p = {0};
    char err[256];
    if (!initialize(&p))
        exit(2);
    dc_gpu_t gpu = {.device = p.device,
                    .physical = p.physical,
                    .width = 64,
                    .height = 64,
                    .page_width = 1,
                    .page_height = 1};
    VkBuffer body;
    VkDeviceMemory body_memory;
    if (!dc_gpu_make_mapped_buffer(&gpu, dc_gpu_body_storage_bytes(&gpu), &body,
                                   &body_memory, &gpu.body_mapped, err,
                                   sizeof(err)) ||
        !dc_gpu_make_mapped_buffer(&gpu, 39u * sizeof(uint32_t), &p.buffer,
                                   &p.memory, (void **)&p.mapped, err,
                                   sizeof(err))) {
        printf("buffer failure: %s\n", err);
        exit(2);
    }
    dc_gpu_structural_data(&gpu)[1] = DC_GPU_CHUNK_SLOTS;
    dc_gpu_structural_data(&gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = dirty;
    for (unsigned i = 0; i < 39; i++)
        p.mapped[i] = 4369u;
    const uint32_t bindings[] = {1, 4, 27, 28, 29, 30};
    if (!fixture_body_pipeline(&p, &gpu, body,
                               "build/diagnostics/component_extract_probe.spv",
                               bindings, 6, 26) ||
        !begin(&p))
        exit(2);
    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                                VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p.command, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier,
                         0, NULL, 0, NULL);
    vkCmdBindPipeline(p.command, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
    vkCmdBindDescriptorSets(p.command, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout,
                            0, 1, &p.descriptor, 0, NULL);
    uint32_t push[8] = {64, 64, 26, 0, 0, 0, 0, 0};
    vkCmdPushConstants(p.command, p.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), push);
    /* Poisoned triplets are inspected, never submitted as indirect dispatches.
     */
    vkCmdDispatch(p.command, 1, 1, 1);
    host_barrier(&p, 1);
    if (!check(vkEndCommandBuffer(p.command), "phase26 end") ||
        !submit(&p, "phase26 fence", 1))
        exit(2);
    bool safe = true;
    const unsigned owned[] = {27, 30};
    for (unsigned i = 0; i < 2; i++) {
        unsigned offset = owned[i];
        printf("dirty%u triplet%u: %u,%u,%u (expected0,1,1)\n", dirty, offset,
               p.mapped[offset], p.mapped[offset + 1], p.mapped[offset + 2]);
        safe &= p.mapped[offset] == 0u && p.mapped[offset + 1] == 1u &&
                p.mapped[offset + 2] == 1u;
    }
    vkDestroyPipeline(p.device, p.pipeline, NULL);
    vkDestroyShaderModule(p.device, p.shader, NULL);
    vkDestroyPipelineLayout(p.device, p.layout, NULL);
    vkDestroyDescriptorPool(p.device, p.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(p.device, p.descriptor_layout, NULL);
    vkUnmapMemory(p.device, p.memory);
    vkDestroyBuffer(p.device, p.buffer, NULL);
    vkFreeMemory(p.device, p.memory, NULL);
    vkUnmapMemory(p.device, body_memory);
    vkDestroyBuffer(p.device, body, NULL);
    vkFreeMemory(p.device, body_memory, NULL);
    vkDestroyFence(p.device, p.fence, NULL);
    vkDestroyCommandPool(p.device, p.pool, NULL);
    vkDestroyDevice(p.device, NULL);
    vkDestroyInstance(p.instance, NULL);
    return safe;
}
static void test_disabled_extraction_discards_grain_tile_ids(void) {
    ASSERT_TRUE(initializes_owned_triplets(false));
    PASS();
}
static void
test_enabled_extraction_resets_dynamic_triplets_before_planning(void) {
    ASSERT_TRUE(initializes_owned_triplets(true));
    PASS();
}
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    RUN(test_disabled_extraction_discards_grain_tile_ids);
    RUN(test_enabled_extraction_resets_dynamic_triplets_before_planning);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
