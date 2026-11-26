#define main carve_probe_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

static int g_pass, g_fail;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %d: %s\n", __LINE__, #expr); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static void test_stale_root_keeps_an_accepted_carve_edge(void) {
    probe_t probe = {0};
    ASSERT_TRUE(initialize(&probe) && allocate_buffer(&probe));
    ASSERT_TRUE(create_pipeline(&probe, "build/diagnostics/carve_union_probe.spv") && begin(&probe));
    vkCmdBindPipeline(probe.command, VK_PIPELINE_BIND_POINT_COMPUTE, probe.pipeline);
    vkCmdBindDescriptorSets(probe.command, VK_PIPELINE_BIND_POINT_COMPUTE, probe.layout,
                           0, 1, &probe.descriptor, 0, NULL);
    vkCmdDispatch(probe.command, 1, 1, 1);
    host_barrier(&probe, 1);
    ASSERT_TRUE(check(vkEndCommandBuffer(probe.command), "carve union command end") &&
                submit(&probe, "carve union fence", 1));

    uint32_t observed = probe.mapped[3], retained = probe.mapped[4], roots[3];
    memcpy(roots, probe.mapped + 5, sizeof(roots));
    printf("Stale link observed parent %u, retained parent %u, final roots %u/%u/%u\n",
           observed, retained, roots[0], roots[1], roots[2]);
    vkDestroyPipeline(probe.device, probe.pipeline, NULL);
    vkDestroyShaderModule(probe.device, probe.shader, NULL);
    vkDestroyPipelineLayout(probe.device, probe.layout, NULL);
    vkDestroyDescriptorPool(probe.device, probe.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(probe.device, probe.descriptor_layout, NULL);
    vkUnmapMemory(probe.device, probe.memory);
    vkDestroyBuffer(probe.device, probe.buffer, NULL);
    vkFreeMemory(probe.device, probe.memory, NULL);
    vkDestroyFence(probe.device, probe.fence, NULL);
    vkDestroyCommandPool(probe.device, probe.pool, NULL);
    vkDestroyDevice(probe.device, NULL);
    vkDestroyInstance(probe.instance, NULL);

    ASSERT_EQ(observed, 2u);
    ASSERT_EQ(retained, 2u);
    ASSERT_EQ(roots[0], 0u);
    ASSERT_EQ(roots[1], 0u);
    ASSERT_EQ(roots[2], 0u);
    PASS();
}

int main(void) {
    RUN(test_stale_root_keeps_an_accepted_carve_edge);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
