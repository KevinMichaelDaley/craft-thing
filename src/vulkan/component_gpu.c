#include "gpu_internal.h"

bool dc_gpu_enable_automatic_components(dc_gpu_t *gpu) {
    if (!gpu)
        return false;
    gpu->automatic_components_enabled = true;
    gpu->structural_enabled = true;
    dc_gpu_structural_data(gpu)[19] = 1;
    dc_gpu_structural_data(gpu)[20] = 1;
    return dc_gpu_set_rigid_solver(gpu, true);
}

void dc_gpu_record_component_extraction(dc_gpu_t *gpu) {
    dc_gpu_record_components(gpu, true);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->component_extract_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0,
                            NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width,  gpu->height,        0,
                        0,           (uint32_t)x,        (uint32_t)(x >> 32),
                        (uint32_t)y, (uint32_t)(y >> 32)};
    push[2] = 1;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command,
                  ((gpu->width * gpu->height + 31u) / 32u + 127u) / 128u, 1, 1);
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    push[2] = 0;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1, 1, 1);
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}
