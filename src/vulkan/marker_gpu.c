#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_marker_t) == 16, "GPU marker layout must match GLSL");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_marker_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize markers = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
                           DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t);
    VkDeviceSize counts = DC_GPU_CHUNK_SLOTS * sizeof(uint32_t);
    VkDeviceSize grid = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
                        DC_CHUNK_CELLS * sizeof(uint32_t);
    gpu->marker_correction = true;
    bool okay = dc_gpu_make_mapped_buffer(gpu, markers, &gpu->marker_a_buffer,
               &gpu->marker_a_memory, &gpu->marker_a_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, markers, &gpu->marker_b_buffer,
               &gpu->marker_b_memory, &gpu->marker_b_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, counts, &gpu->marker_count_a_buffer,
               &gpu->marker_count_a_memory, &gpu->marker_count_a_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, counts, &gpu->marker_count_b_buffer,
               &gpu->marker_count_b_memory, &gpu->marker_count_b_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid, &gpu->marker_grid_buffer,
               &gpu->marker_grid_memory, &gpu->marker_grid_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, counts, &gpu->slot_page_buffer,
               &gpu->slot_page_memory, &gpu->slot_page_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, counts, &gpu->slot_seed_buffer,
               &gpu->slot_seed_memory, &gpu->slot_seed_mapped, err, cap);
    if (okay) memcpy(gpu->slot_page_mapped, gpu->slot_page, sizeof(gpu->slot_page));
    return okay;
}

bool dc_gpu_marker_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/marker.comp.spv",
                                   &module, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                                1, &info, NULL, &gpu->marker_pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create marker pipeline");
    return true;
}

void dc_gpu_marker_destroy(dc_gpu_t *gpu) {
    if (gpu->marker_pipeline) vkDestroyPipeline(gpu->device, gpu->marker_pipeline, NULL);
#define RELEASE(name) do { \
    if (gpu->name##_mapped) vkUnmapMemory(gpu->device, gpu->name##_memory); \
    if (gpu->name##_buffer) vkDestroyBuffer(gpu->device, gpu->name##_buffer, NULL); \
    if (gpu->name##_memory) vkFreeMemory(gpu->device, gpu->name##_memory, NULL); \
} while (0)
    RELEASE(marker_a);
    RELEASE(marker_b);
    RELEASE(marker_count_a);
    RELEASE(marker_count_b);
    RELEASE(marker_grid);
    RELEASE(slot_page);
    RELEASE(slot_seed);
#undef RELEASE
}

static void marker_barrier(dc_gpu_t *gpu) {
    VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_markers(dc_gpu_t *gpu) {
    marker_barrier(gpu);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->marker_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0, gpu->marker_ping,
                         gpu->fluid_tick, 0, 0 };
    const uint32_t groups[4] = {
        DC_GPU_CHUNK_SLOTS * DC_MARKERS_PER_CHUNK / 256u,
        DC_GPU_CHUNK_SLOTS * DC_MARKERS_PER_CHUNK / 256u,
        (gpu->width * gpu->height + 255u) / 256u,
        1u
    };
    for (uint32_t mode = 0; mode < 4; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, groups[mode], 1, 1);
        marker_barrier(gpu);
    }
    gpu->marker_ping ^= 1u;
}

bool dc_gpu_set_marker_correction(dc_gpu_t *gpu, bool enabled) {
    if (!gpu) return false;
    gpu->marker_correction = enabled;
    return true;
}

bool dc_gpu_set_marker_overlay(dc_gpu_t *gpu, bool enabled) {
    if (!gpu) return false;
    gpu->marker_overlay = enabled;
    return true;
}

bool dc_gpu_marker_count(dc_gpu_t *gpu, uint32_t slot, uint32_t *count) {
    if (!gpu || !count || slot >= DC_GPU_CHUNK_SLOTS) return false;
    const uint32_t *counts = gpu->marker_ping ?
        gpu->marker_count_b_mapped : gpu->marker_count_a_mapped;
    *count = counts[slot];
    return *count <= DC_MARKERS_PER_CHUNK;
}
