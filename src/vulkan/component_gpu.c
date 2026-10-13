#include "gpu_internal.h"

bool dc_gpu_enable_automatic_components(dc_gpu_t *gpu) {
    if (!gpu)
        return false;
    gpu->automatic_components_enabled = true;
    gpu->structural_enabled = true;
    dc_gpu_structural_data(gpu)[19] = 1;
    dc_gpu_structural_data(gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = 1;
    return dc_gpu_set_rigid_solver(gpu, true);
}

static void extraction_stage(dc_gpu_t *gpu, uint32_t push[8], uint32_t mode,
                             uint32_t stride, uint32_t groups) {
    push[2] = mode;
    push[3] = stride;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, 8 * sizeof(uint32_t), push);
    if (mode == 26)
        vkCmdDispatch(gpu->command, 1, 1, 1);
    else
        vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                               (mode == 9 ? 15u : mode >= 11 && mode <= 13 ? 12u :
                                groups == 1 ? 3u : groups == 64 ? 6u : 0u) *
                                   sizeof(uint32_t));
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_component_extraction(dc_gpu_t *gpu) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->component_extract_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width, gpu->height, 0, 0,
                        (uint32_t)x, (uint32_t)(x >> 32),
                        (uint32_t)y, (uint32_t)(y >> 32)};
    uint32_t cells = DC_GPU_CHUNK_SLOTS * 4096u, groups = (cells + 127u) / 128u;
    extraction_stage(gpu, push, 26, 0, 1);
    extraction_stage(gpu, push, 9, 0, (DC_GPU_CHUNK_SLOTS * 4u + 127u) / 128u);
    extraction_stage(gpu, push, 10, 0, groups);
    for (uint32_t stride = 1; stride < cells; stride *= 2) {
        for (unsigned round = 0; round < 2; ++round) {
            extraction_stage(gpu, push, 11, 0, groups);
            extraction_stage(gpu, push, 12, 0, groups);
            extraction_stage(gpu, push, 13, 0, groups);
            extraction_stage(gpu, push, 27, 0, 1);
        }
    }
    extraction_stage(gpu, push, 14, 0, groups);
    extraction_stage(gpu, push, 1, 0, groups);
    for (uint32_t stride = 1; stride < cells; stride *= 2) {
        extraction_stage(gpu, push, 2, stride, groups);
        extraction_stage(gpu, push, 3, 0, groups);
    }
    extraction_stage(gpu, push, 4, 0, groups);
    extraction_stage(gpu, push, 5, 0, groups);
    extraction_stage(gpu, push, 6, 0, 1);
    extraction_stage(gpu, push, 7, 0, groups);
    extraction_stage(gpu, push, 8, 0, 1);
    extraction_stage(gpu, push, 15, 0, groups);
    for (uint32_t stride = 1; stride < cells; stride *= 2) {
        extraction_stage(gpu, push, 19, stride, groups);
        extraction_stage(gpu, push, 20, 0, groups);
    }
    extraction_stage(gpu, push, 16, 0, 1);
    extraction_stage(gpu, push, 17, 0, 64);
    extraction_stage(gpu, push, 18, 0, 1);
    extraction_stage(gpu, push, 21, 0, 1);
    extraction_stage(gpu, push, 22, 0, 64);
    extraction_stage(gpu, push, 28, 0, groups);
    extraction_stage(gpu, push, 23, 0, 64);
    extraction_stage(gpu, push, 24, 0, groups);
    extraction_stage(gpu, push, 25, 0, 1);
}
