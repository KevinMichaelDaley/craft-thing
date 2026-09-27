#include <stdio.h>

#include "gpu_internal.h"

enum { DC_MPM_SUBSTEPS = 2, DC_MPM_MODES = 6 };

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_mpm_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize particles = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    VkDeviceSize flags = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(uint32_t);
    VkDeviceSize grid = (VkDeviceSize)gpu->width * gpu->height;
    VkDeviceSize tiles = (VkDeviceSize)((gpu->width + 15u) / 16u) *
                         ((gpu->height + 15u) / 16u);
    return dc_gpu_make_mapped_buffer(gpu, particles, &gpu->mpm_proposal_buffer,
               &gpu->mpm_proposal_memory, &gpu->mpm_proposal_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, particles, &gpu->mpm_output_buffer,
               &gpu->mpm_output_memory, &gpu->mpm_output_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u, &gpu->mpm_grid_buffer,
               &gpu->mpm_grid_memory, &gpu->mpm_grid_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u, &gpu->mpm_force_buffer,
               &gpu->mpm_force_memory, &gpu->mpm_force_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 8u, &gpu->mpm_velocity_buffer,
               &gpu->mpm_velocity_memory, &gpu->mpm_velocity_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, flags, &gpu->mpm_accept_buffer,
               &gpu->mpm_accept_memory, &gpu->mpm_accept_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, (3u + 2u * tiles) * sizeof(uint32_t),
               &gpu->mpm_activity_buffer, &gpu->mpm_activity_memory,
               &gpu->mpm_activity_mapped, err, cap);
}

static bool make_pipeline(dc_gpu_t *gpu, const char *path, VkPipeline *pipeline,
                          char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &module, err, cap)) return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                               1, &info, NULL, pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create MPM pipeline");
    return true;
}

bool dc_gpu_mpm_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return make_pipeline(gpu, "build/shaders/mpm.comp.spv", &gpu->mpm_pipeline,
                         err, cap) &&
           make_pipeline(gpu, "build/shaders/mpm_active.comp.spv",
                         &gpu->mpm_activity_pipeline, err, cap);
}

void dc_gpu_mpm_pipeline_destroy(dc_gpu_t *gpu) {
    if (gpu->mpm_pipeline) vkDestroyPipeline(gpu->device, gpu->mpm_pipeline, NULL);
    if (gpu->mpm_activity_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_activity_pipeline, NULL);
}

#define DESTROY_MPM_BUFFER(name) do { \
    if (gpu->mpm_##name##_mapped) vkUnmapMemory(gpu->device, gpu->mpm_##name##_memory); \
    if (gpu->mpm_##name##_buffer) vkDestroyBuffer(gpu->device, gpu->mpm_##name##_buffer, NULL); \
    if (gpu->mpm_##name##_memory) vkFreeMemory(gpu->device, gpu->mpm_##name##_memory, NULL); \
} while (0)

void dc_gpu_mpm_buffers_destroy(dc_gpu_t *gpu) {
    DESTROY_MPM_BUFFER(proposal);
    DESTROY_MPM_BUFFER(output);
    DESTROY_MPM_BUFFER(grid);
    DESTROY_MPM_BUFFER(force);
    DESTROY_MPM_BUFFER(velocity);
    DESTROY_MPM_BUFFER(accept);
    DESTROY_MPM_BUFFER(activity);
}

void dc_gpu_record_mpm(dc_gpu_t *gpu) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_activity_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0u, 0u, 0u, 0u, 0u };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
                  (gpu->height + 15u) / 16u, 1u);
    VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    push[2] = 1u;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1u, 1u, 1u);
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_pipeline);
    for (uint32_t step = 0; step < DC_MPM_SUBSTEPS; ++step) {
        for (uint32_t mode = 0; mode < DC_MPM_MODES; ++mode) {
            push[2] = mode;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
            vkCmdPipelineBarrier2(gpu->command, &dependency);
        }
    }
}
