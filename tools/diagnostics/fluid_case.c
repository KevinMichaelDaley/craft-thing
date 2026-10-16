#define main fluid_full_suite_main
#include "../../tests/vulkan/fluid_tests.c"
#undef main
#include <string.h>

static void test_completed_heads_match_closed_pool(void) {
    char err[256];
    dc_chunk_t left = {0}, right = {0};
    for (unsigned y = 10; y < 20; y++)
        for (unsigned x = 10; x < 20; x++)
            left.cells[y * 64 + x].fluid_mass = DC_FLUID_FULL;
    for (unsigned y = 9; y <= 20; y++) {
        left.cells[y * 64 + 9].material = DC_MATERIAL_STONE;
        left.cells[y * 64 + 20].material = DC_MATERIAL_STONE;
    }
    for (unsigned x = 9; x <= 20; x++) {
        left.cells[9 * 64 + x].material = DC_MATERIAL_STONE;
        left.cells[20 * 64 + x].material = DC_MATERIAL_STONE;
    }
    dc_gpu_t *g = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(g);
    VkBuffer out;
    VkDeviceMemory memory;
    uint32_t *words;
    unsigned n = g->width * g->height;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g, n * 8u, &out, &memory,
                                          (void **)&words, err, sizeof(err)));
    ASSERT_EQ(vkResetCommandBuffer(g->command, 0), VK_SUCCESS);
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ASSERT_EQ(vkBeginCommandBuffer(g->command, &begin), VK_SUCCESS);
    dc_gpu_record_fluid_phase(g, 0);
    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
    vkCmdPipelineBarrier(g->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0,
                         NULL, 0, NULL);
    VkBufferCopy copy = {.size = n * 4u};
    vkCmdCopyBuffer(g->command, g->fluid_b_buffer, out, 1, &copy);
    copy.dstOffset = n * 4u;
    vkCmdCopyBuffer(g->command, g->pressure_a_buffer, out, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(g->command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, NULL, 0,
                         NULL);
    ASSERT_EQ(vkEndCommandBuffer(g->command), VK_SUCCESS);
    VkFenceCreateInfo fence_info = {.sType =
                                        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    ASSERT_EQ(vkCreateFence(g->device, &fence_info, NULL, &fence), VK_SUCCESS);
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &g->command};
    ASSERT_EQ(vkQueueSubmit(g->queue, 1, &submit, fence), VK_SUCCESS);
    ASSERT_EQ(
        vkWaitForFences(g->device, 1, &fence, VK_TRUE, UINT64_C(1000000000)),
        VK_SUCCESS);
    unsigned mismatches = 0;
    for (unsigned y = 0; y < g->height; y++)
        for (unsigned x = 0; x < g->width; x++) {
            unsigned expected = x >= 10 && x < 20 && y >= 10 && y < 20
                                    ? (y - 9u) * DC_FLUID_FULL
                                    : 0u,
                     index = y * g->width + x;
            if (words[index] != expected) {
                if (mismatches < 8)
                    printf("head %u,%u=%u expected%u pressure%.9g\n", x, y,
                           words[index], expected,
                           ((float *)(words + n))[index]);
                mismatches++;
            }
        }
    printf("completed heads mismatches%u; row10 head%u pressure%.9g\n",
           mismatches, words[10 * g->width + 10],
           ((float *)(words + n))[10 * g->width + 10]);
    vkDestroyFence(g->device, fence, NULL);
    vkUnmapMemory(g->device, memory);
    vkDestroyBuffer(g->device, out, NULL);
    vkFreeMemory(g->device, memory, NULL);
    dc_gpu_destroy(g);
    ASSERT_EQ(mismatches, 0u);
    PASS();
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    if (strcmp(argv[1], "heads") == 0)
        RUN(test_completed_heads_match_closed_pool);
    else if (strcmp(argv[1], "displacement") == 0)
        RUN(test_rigid_displacement_raises_water_without_losing_volume);
    else if (strcmp(argv[1], "pressure") == 0)
        RUN(test_closed_liquid_velocity_is_projected);
    else if (strcmp(argv[1], "unknown") == 0)
        RUN(test_unloaded_neighbor_keeps_mass_in_source);
    else if (strcmp(argv[1], "phases") == 0)
        RUN(test_six_fluid_phases_match_one_uncoupled_update);
    else
        return 2;
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_pass != 1 || g_fail != 0;
}
