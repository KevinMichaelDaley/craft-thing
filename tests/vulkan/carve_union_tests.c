#define DC_PROBE_WORDS 4224
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

static int run_carve_probe(probe_t *probe, const char *path, uint32_t groups) {
    if (!initialize(probe) || !allocate_buffer(probe) || !create_pipeline(probe, path) || !begin(probe))
        return 0;
    vkCmdBindPipeline(probe->command, VK_PIPELINE_BIND_POINT_COMPUTE, probe->pipeline);
    vkCmdBindDescriptorSets(probe->command, VK_PIPELINE_BIND_POINT_COMPUTE, probe->layout,
                           0, 1, &probe->descriptor, 0, NULL);
    vkCmdDispatch(probe->command, groups, 1, 1);
    host_barrier(probe, 1);
    return check(vkEndCommandBuffer(probe->command), "carve union command end") &&
           submit(probe, "carve union fence", 1);
}

static void close_carve_probe(probe_t *probe) {
    vkDestroyPipeline(probe->device, probe->pipeline, NULL);
    vkDestroyShaderModule(probe->device, probe->shader, NULL);
    vkDestroyPipelineLayout(probe->device, probe->layout, NULL);
    vkDestroyDescriptorPool(probe->device, probe->descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(probe->device, probe->descriptor_layout, NULL);
    vkUnmapMemory(probe->device, probe->memory);
    vkDestroyBuffer(probe->device, probe->buffer, NULL);
    vkFreeMemory(probe->device, probe->memory, NULL);
    vkDestroyFence(probe->device, probe->fence, NULL);
    vkDestroyCommandPool(probe->device, probe->pool, NULL);
    vkDestroyDevice(probe->device, NULL);
    vkDestroyInstance(probe->instance, NULL);
}

static void test_stale_root_keeps_an_accepted_carve_edge(void) {
    probe_t probe = {0};
    ASSERT_TRUE(run_carve_probe(&probe, "build/diagnostics/carve_union_probe.spv", 1));

    uint32_t observed = probe.mapped[3], retained = probe.mapped[4], roots[3];
    memcpy(roots, probe.mapped + 5, sizeof(roots));
    printf("Stale link observed parent %u, retained parent %u, final roots %u/%u/%u\n",
           observed, retained, roots[0], roots[1], roots[2]);
    close_carve_probe(&probe);

    ASSERT_EQ(observed, 2u);
    ASSERT_EQ(retained, 2u);
    ASSERT_EQ(roots[0], 0u);
    ASSERT_EQ(roots[1], 0u);
    ASSERT_EQ(roots[2], 0u);
    PASS();
}

static void test_full_tile_connectivity_skips_internal_pixel_edges(void) {
    probe_t probe = {0};
    ASSERT_TRUE(run_carve_probe(&probe, "build/diagnostics/carve_union_tile_probe.spv", 2));
    uint32_t intact[7], split[7];
    memcpy(intact, probe.mapped, sizeof(intact));
    memcpy(split, probe.mapped + 7, sizeof(split));
    close_carve_probe(&probe);

    printf("Diagonal tiles have %u pixels, %u components and %u union edges\n",
           intact[3], intact[1], intact[2]);
    printf("Cut tiles have %u/%u pixels and %u components\n", split[3], split[4], split[1]);
    ASSERT_EQ(intact[0], 1u); ASSERT_EQ(intact[1], 1u); ASSERT_EQ(intact[3], 512u);
    ASSERT_EQ(intact[5], 1u); ASSERT_EQ(intact[6], 1u);
    ASSERT_EQ(split[0], 1u); ASSERT_EQ(split[1], 2u);
    ASSERT_EQ(split[3], 255u); ASSERT_EQ(split[4], 256u);
    ASSERT_EQ(split[5], 1u); ASSERT_EQ(split[6], 2u);
    ASSERT_EQ(intact[2], 1u);
    PASS();
}

int main(void) {
    RUN(test_stale_root_keeps_an_accepted_carve_edge);
    RUN(test_full_tile_connectivity_skips_internal_pixel_edges);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
