#define main fluid_full_suite_main
#include "../../tests/vulkan/fluid_tests.c"
#undef main
#include <string.h>
#include "../../src/vulkan/renderer_internal.h"
static dc_gpu_t *dispatch_gpu;
static VkPipeline dispatch_pipeline;
static uint32_t dispatch_mode;
static uint64_t dispatch_jobs[13];
static bool require_cooperative;
static bool require_displacement;
void __real_vkCmdBindPipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline);
void __real_vkCmdPushConstants(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags,uint32_t,uint32_t,const void *);
void __real_vkCmdDispatch(VkCommandBuffer,uint32_t,uint32_t,uint32_t);
void __wrap_vkCmdBindPipeline(VkCommandBuffer command,VkPipelineBindPoint point,VkPipeline pipeline) {
    if(dispatch_gpu&&command==dispatch_gpu->command&&point==VK_PIPELINE_BIND_POINT_COMPUTE) dispatch_pipeline=pipeline;
    __real_vkCmdBindPipeline(command,point,pipeline);
}
void __wrap_vkCmdPushConstants(VkCommandBuffer command,VkPipelineLayout layout,VkShaderStageFlags flags,
                               uint32_t offset,uint32_t size,const void *data) {
    if(dispatch_gpu&&command==dispatch_gpu->command&&offset==0&&size>=12) memcpy(&dispatch_mode,(const char *)data+8,4);
    __real_vkCmdPushConstants(command,layout,flags,offset,size,data);
}
void __wrap_vkCmdDispatch(VkCommandBuffer command,uint32_t x,uint32_t y,uint32_t z) {
    if(dispatch_gpu&&command==dispatch_gpu->command&&dispatch_mode<13&&dispatch_pipeline!=VK_NULL_HANDLE&&
       (dispatch_pipeline==dispatch_gpu->fluid_pipeline||dispatch_pipeline==dispatch_gpu->fluid_phase_pipelines[dispatch_mode]||
        (dispatch_mode<4&&dispatch_pipeline==dispatch_gpu->fluid_transport_pipelines[dispatch_mode])||
        (dispatch_mode==11&&dispatch_pipeline==dispatch_gpu->fluid_displacement_pipelines[0])||
        (dispatch_mode==12&&dispatch_pipeline==dispatch_gpu->fluid_displacement_pipelines[1]))) dispatch_jobs[dispatch_mode]+=(uint64_t)x*y*z;
    __real_vkCmdDispatch(command,x,y,z);
}

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
static void test_fluid_variant_fingerprint(void) {
    char err[256];
    dc_chunk_t left = {0}, right = {0};
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++) {
            left.cells[y * 64 + x].material =
                (x == 0 || y == 63) ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
            right.cells[y * 64 + x].material =
                (x == 63 || y == 63) ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
            if (y >= 28 && y < 60) {
                left.cells[y * 64 + x].fluid_mass =
                    (x > 3 && x < 63) ? DC_FLUID_FULL - (x * 73 + y * 97) % 8192
                                      : 0;
                right.cells[y * 64 + x].fluid_mass =
                    x < 60 ? DC_FLUID_FULL - (x * 101 + y * 41) % 8192 : 0;
            }
        }
    dc_gpu_t *g = make_grid(&left, &right, err, sizeof(err));
    ASSERT_TRUE(g);
    const char *variant = getenv("DC_FLUID_VARIANTS");
    if (variant && strcmp(variant, "specialized") == 0) {
        for (unsigned p = 0; p < 13; p++)
            ASSERT_TRUE(g->fluid_phase_pipelines[p] != VK_NULL_HANDLE);
        for (unsigned p = 0; p < 6; p++)
            if (p != 4)
                ASSERT_TRUE(g->projection_phase_pipelines[p] != VK_NULL_HANDLE);
    }
    for (unsigned tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_fluid_step(g, err, sizeof(err)));
    uint64_t hash = UINT64_C(14695981039346656037), mass = 0;
    dc_chunk_t saved;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g, slot, &saved, err, sizeof(err)));
        const unsigned char *bytes = (void *)saved.cells;
        for (size_t i = 0; i < sizeof(saved.cells); i++)
            hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
        bytes = (void *)saved.face_velocity;
        for (size_t i = 0; i < sizeof(saved.face_velocity); i++)
            hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
        for (unsigned i = 0; i < DC_CHUNK_CELLS; i++)
            mass += saved.cells[i].fluid_mass;
    }
    printf("fluid variant %s fingerprint %016llx mass %llu\n",
           variant ? variant : "production", (unsigned long long)hash,
           (unsigned long long)mass);
    dc_gpu_destroy(g);
    PASS();
}
static bool read_aperture_prefix(dc_gpu_t *g, uint32_t *prefix, size_t bytes,
                                 char *err, uint32_t cap) {
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    bool ok = dc_gpu_make_staging_buffer(g, bytes, &staging, &memory, &mapped, err, cap);
    if (ok && (ok = fr_renderer_begin(g))) {
        fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferCopy copy = {.size = bytes};
        vkCmdCopyBuffer(g->command, g->aperture_prefix_buffer, staging, 1, &copy);
        fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                            VK_ACCESS_2_HOST_READ_BIT);
        ok = fr_renderer_submit(g);
        if (ok) memcpy(prefix, mapped, bytes);
    }
    if (mapped) vkUnmapMemory(g->device, memory);
    if (staging) vkDestroyBuffer(g->device, staging, NULL);
    if (memory) vkFreeMemory(g->device, memory, NULL);
    return ok;
}

#include "../../tests/vulkan/fluid_aperture_runtime_case.h"

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    if (strcmp(argv[1], "displacement-runtime") == 0) {
        require_cooperative=true;require_displacement=true;
        RUN(test_long_transport_uses_exact_runtime_apertures);
    } else if (strcmp(argv[1], "cooperative-runtime") == 0) {
        require_cooperative=true;
        RUN(test_long_transport_uses_exact_runtime_apertures);
    } else if (strcmp(argv[1], "aperture-runtime") == 0)
        RUN(test_long_transport_uses_exact_runtime_apertures);
    else if (strcmp(argv[1], "variant") == 0)
        RUN(test_fluid_variant_fingerprint);
    else if (strcmp(argv[1], "heads") == 0)
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
