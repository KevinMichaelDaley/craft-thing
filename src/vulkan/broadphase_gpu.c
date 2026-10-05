#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(DC_GPU_BODY_CAPACITY == 64, "Broadphase shader body capacity must match");
_Static_assert(sizeof(dc_gpu_broadphase_pair_t) == 32, "Broadphase pair layout must match");
_Static_assert(sizeof(dc_gpu_broadphase_stats_t) == 24, "Broadphase counters must match");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_broadphase_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/broadphase.comp.spv", &module, err, cap))
        return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
                                              &info, NULL, &gpu->broadphase_pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS ? true : error(err, cap, "Cannot create GPU broadphase pipeline");
}

bool dc_gpu_set_broadphase_capacity(dc_gpu_t *gpu, uint32_t capacity,
                                    char *err, uint32_t cap) {
    if (!gpu || capacity > DC_GPU_BROADPHASE_PAIR_CAPACITY)
        return error(err, cap, "Invalid broadphase capacity");
    dc_gpu_broadphase_data(gpu)[3] = capacity;
    gpu->body_refresh_pending = true;
    return true;
}

bool dc_gpu_read_broadphase(dc_gpu_t *gpu, dc_gpu_broadphase_stats_t *stats,
                            dc_gpu_broadphase_pair_t *pairs, uint32_t pair_cap,
                            char *err, uint32_t cap) {
    if (!gpu || !stats || (!pairs && pair_cap))
        return error(err, cap, "Invalid broadphase readback");
    const uint32_t *data = dc_gpu_broadphase_data(gpu);
    dc_gpu_broadphase_stats_t completed;
    memcpy(&completed, data, sizeof(completed));
    if (completed.count > DC_GPU_BROADPHASE_PAIR_CAPACITY ||
        (pairs && pair_cap < completed.count))
        return error(err, cap, "Broadphase readback buffer is too small");
    if (pairs) {
        uint32_t offset = DC_GPU_BROADPHASE_HEADER_WORDS +
            DC_GPU_BROADPHASE_BUCKET_WORDS * dc_gpu_broadphase_buckets(gpu);
        memcpy(pairs, data + offset, (size_t)completed.count * sizeof(*pairs));
    }
    *stats = completed;
    return true;
}

static void barrier(dc_gpu_t *gpu, bool complete) {
    VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                        (complete ? VK_PIPELINE_STAGE_2_HOST_BIT : 0),
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                        (complete ? VK_ACCESS_2_HOST_READ_BIT : 0) };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_broadphase(dc_gpu_t *gpu) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->broadphase_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = { gpu->width, gpu->height, 0, gpu->body_count,
        (uint32_t)x, (uint32_t)(x >> 32), (uint32_t)y, (uint32_t)(y >> 32) };
    uint32_t buckets = dc_gpu_broadphase_buckets(gpu);
    for (uint32_t mode = 0; mode < 3; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        uint32_t work = mode == 0 ? 2u * buckets : mode == 1 ? DC_GPU_BODY_CAPACITY : buckets;
        vkCmdDispatch(gpu->command, (work + 63u) / 64u, 1, 1);
        barrier(gpu, mode == 2);
    }
}
