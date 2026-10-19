#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static char g_error[256];
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s (%s)\n", __LINE__, #e, g_error); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static bool triplet(dc_gpu_t *gpu, uint32_t word, uint32_t output[3]) {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    uint64_t local = gpu->mapped_local_bytes, system = gpu->mapped_system_bytes;
    if (!dc_gpu_make_mapped_buffer(gpu, 12, &buffer, &memory, &mapped, g_error, sizeof(g_error))) return false;
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bool okay = vkResetCommandBuffer(gpu->command, 0) == VK_SUCCESS &&
        vkBeginCommandBuffer(gpu->command, &begin) == VK_SUCCESS;
    if (okay) {
        VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT};
        VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(gpu->command, &dependency);
        VkBufferCopy copy = {(VkDeviceSize)word * 4u, 0, 12};
        vkCmdCopyBuffer(gpu->command, gpu->mpm_activity_buffer, buffer, 1, &copy);
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        vkCmdPipelineBarrier2(gpu->command, &dependency);
        okay = vkEndCommandBuffer(gpu->command) == VK_SUCCESS;
    }
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command};
    if (okay) okay = vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
        vkQueueWaitIdle(gpu->queue) == VK_SUCCESS;
    if (okay) memcpy(output, mapped, 12);
    vkUnmapMemory(gpu->device, memory);
    vkDestroyBuffer(gpu->device, buffer, NULL);
    vkFreeMemory(gpu->device, memory, NULL);
    gpu->mapped_local_bytes = local; gpu->mapped_system_bytes = system;
    return okay;
}

static dc_gpu_t *context(bool grains) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = chunk && dc_gpu_create(&gpu, 64, 64,
        "build/shaders/pattern.comp.spv", g_error, sizeof(g_error));
    if (grains && chunk) chunk->cells[50 * 64 + 40].material = DC_MATERIAL_DIRT;
    if (okay) okay = dc_gpu_upload_chunk(gpu, 0, chunk, g_error, sizeof(g_error)) &&
        dc_gpu_set_page(gpu, 0, 0, 0, g_error, sizeof(g_error)) &&
        dc_gpu_enable_automatic_components(gpu);
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static bool spawn(dc_gpu_t *gpu) {
    dc_gpu_body_t body = {.x_fp = 8 * 65536, .y_fp = 8 * 65536,
        .width = 4, .height = 4, .id = 1, .active = 1};
    return dc_gpu_spawn_body(gpu, body, g_error, sizeof(g_error));
}

static void test_body_without_particles_has_no_grain_bed_work(void) {
    dc_gpu_t *gpu = context(false); ASSERT_TRUE(gpu);
    ASSERT_TRUE(spawn(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    uint32_t work[3]; ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 0u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(triplet(gpu, DC_RIGID_BODY_WORK_WORD, work)); ASSERT_EQ(work[0], 1u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_grain_bed_work_tracks_body_activation_on_gpu(void) {
    dc_gpu_t *gpu = context(true); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    uint32_t work[3]; ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 0u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(triplet(gpu, DC_RIGID_BODY_WORK_WORD, work)); ASSERT_EQ(work[0], 0u);
    ASSERT_TRUE(spawn(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work));
    ASSERT_EQ(work[0], 64u); ASSERT_EQ(work[1], 1u); ASSERT_EQ(work[2], 1u);
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 1, g_error, sizeof(g_error)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, g_error, sizeof(g_error)));
    ASSERT_TRUE(triplet(gpu, DC_GRAIN_BED_WORK_WORD, work)); ASSERT_EQ(work[0], 0u);
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_body_without_particles_has_no_grain_bed_work);
    RUN(test_grain_bed_work_tracks_body_activation_on_gpu);
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
