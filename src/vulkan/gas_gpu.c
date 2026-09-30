#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_gas_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/gas.comp.spv",
                                   &module, err, cap)) return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module,
            .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                               1u, &info, NULL, &gpu->gas_pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create GPU gas pipeline");
    return true;
}

void dc_gpu_gas_pipeline_destroy(dc_gpu_t *gpu) {
    if (gpu->gas_pipeline)
        vkDestroyPipeline(gpu->device, gpu->gas_pipeline, NULL);
}

void dc_gpu_record_gas(dc_gpu_t *gpu) {
    if (!gpu->gas_active) return;
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->gas_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0u, 1u, &gpu->descriptor, 0u, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0u,
                         gpu->gas_tick++ & 1u, 0u, 0u, 0u };
    for (uint32_t mode = 0u; mode < 3u; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(push), push);
        vkCmdDispatch(gpu->command, 4u, 4u, gpu->slot_capacity);
        VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
        VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1u, .pMemoryBarriers = &barrier };
        vkCmdPipelineBarrier2(gpu->command, &dependency);
    }
}
