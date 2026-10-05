#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_contact_t) == 128, "GPU contact layout must match SPIR-V");
_Static_assert(offsetof(dc_gpu_contact_t, anchor_a) == 64, "GPU contact anchor offset changed");
_Static_assert(sizeof(dc_gpu_contact_stats_t) == 24, "GPU contact counters must match SPIR-V");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_set_contact_capacity(dc_gpu_t *gpu, uint32_t capacity, char *err, uint32_t cap) {
    if (!gpu || capacity > DC_GPU_CONTACT_CAPACITY)
        return error(err, cap, "Invalid contact capacity");
    dc_gpu_contact_data(gpu)[3] = capacity;
    gpu->body_refresh_pending = true;
    return true;
}

bool dc_gpu_read_contacts(dc_gpu_t *gpu, dc_gpu_contact_stats_t *stats,
                          dc_gpu_contact_t *contacts, uint32_t contact_cap,
                          char *err, uint32_t cap) {
    if (!gpu || !stats || (!contacts && contact_cap))
        return error(err, cap, "Invalid contact readback");
    const uint32_t *data = dc_gpu_contact_data(gpu);
    dc_gpu_contact_stats_t completed;
    memcpy(&completed, data, sizeof(completed));
    if (completed.count > DC_GPU_CONTACT_CAPACITY || (contacts && contact_cap < completed.count))
        return error(err, cap, "Contact readback buffer is too small");
    if (contacts) memcpy(contacts, data + 8, (size_t)completed.count * sizeof(*contacts));
    *stats = completed;
    return true;
}

bool dc_gpu_contact_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/contacts.comp.spv", &module, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
                                               &info, NULL, &gpu->contact_pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS ? true : error(err, cap, "Cannot create GPU contact pipeline");
}

void dc_gpu_record_contacts(dc_gpu_t *gpu) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->contact_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width, gpu->height, 0, gpu->body_count,
        (uint32_t)x, (uint32_t)(x >> 32), (uint32_t)y, (uint32_t)(y >> 32)};
    for (uint32_t mode = 0; mode < 2; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, mode ? DC_GPU_BROADPHASE_PAIR_CAPACITY / 64u : 1u, 1, 1);
        VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | (mode ? VK_PIPELINE_STAGE_2_HOST_BIT : 0),
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                             (mode ? VK_ACCESS_2_HOST_READ_BIT : 0) };
        VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
        vkCmdPipelineBarrier2(gpu->command, &dependency);
    }
}
