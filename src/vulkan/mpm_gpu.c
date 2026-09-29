#include <math.h>
#include <stdio.h>

#include "gpu_internal.h"

enum { DC_MPM_SUBSTEPS = 2, DC_MPM_MODES = 6,
       DC_MPM_WATER_FEEDBACK_MODE = 6, DC_MPM_MOISTURE_MODE = 7 };

static uint32_t component_round_limit(uint32_t width, uint32_t height) {
    uint64_t cells = (uint64_t)width * height;
    uint32_t rounds = 1u;
    while (cells > 1u) {
        cells = (cells + 1u) / 2u;
        ++rounds;
    }
    return rounds;
}

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_mpm_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize particles = (VkDeviceSize)gpu->slot_capacity *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    VkDeviceSize flags = (VkDeviceSize)gpu->slot_capacity *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(uint32_t);
    VkDeviceSize grid = (VkDeviceSize)gpu->width * gpu->height;
    VkDeviceSize tiles = (VkDeviceSize)((gpu->width + 15u) / 16u) *
                         ((gpu->height + 15u) / 16u);
    return dc_gpu_make_device_buffer(gpu, particles, &gpu->mpm_proposal_buffer,
               &gpu->mpm_proposal_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, particles, &gpu->mpm_output_buffer,
               &gpu->mpm_output_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u, &gpu->mpm_grid_buffer,
               &gpu->mpm_grid_memory, &gpu->mpm_grid_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, grid * 16u, &gpu->mpm_force_buffer,
               &gpu->mpm_force_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u, &gpu->mpm_force_staging_buffer,
               &gpu->mpm_force_staging_memory, &gpu->mpm_force_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, grid * 8u, &gpu->mpm_velocity_buffer,
               &gpu->mpm_velocity_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 8u, &gpu->mpm_velocity_staging_buffer,
               &gpu->mpm_velocity_staging_memory, &gpu->mpm_velocity_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, flags, &gpu->mpm_accept_buffer,
               &gpu->mpm_accept_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, (7u + 2u * tiles) * sizeof(uint32_t),
               &gpu->mpm_activity_buffer, &gpu->mpm_activity_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * sizeof(uint32_t),
               &gpu->mpm_label_a_buffer, &gpu->mpm_label_a_memory,
               &gpu->mpm_label_a_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * sizeof(uint32_t),
               &gpu->mpm_label_b_buffer, &gpu->mpm_label_b_memory,
               &gpu->mpm_label_b_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * sizeof(uint32_t),
               &gpu->mpm_component_size_buffer, &gpu->mpm_component_size_memory,
               &gpu->mpm_component_size_mapped, err, cap);
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
                         &gpu->mpm_activity_pipeline, err, cap) &&
           make_pipeline(gpu, "build/shaders/mpm_component.comp.spv",
                         &gpu->mpm_component_pipeline, err, cap);
}

void dc_gpu_mpm_pipeline_destroy(dc_gpu_t *gpu) {
    if (gpu->mpm_pipeline) vkDestroyPipeline(gpu->device, gpu->mpm_pipeline, NULL);
    if (gpu->mpm_activity_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_activity_pipeline, NULL);
    if (gpu->mpm_component_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_component_pipeline, NULL);
}

#define DESTROY_MPM_BUFFER(name) do { \
    if (gpu->mpm_##name##_buffer) vkDestroyBuffer(gpu->device, gpu->mpm_##name##_buffer, NULL); \
    if (gpu->mpm_##name##_memory) vkFreeMemory(gpu->device, gpu->mpm_##name##_memory, NULL); \
} while (0)

void dc_gpu_mpm_buffers_destroy(dc_gpu_t *gpu) {
    if (gpu->mpm_grid_mapped) vkUnmapMemory(gpu->device, gpu->mpm_grid_memory);
    if (gpu->mpm_label_a_mapped) vkUnmapMemory(gpu->device, gpu->mpm_label_a_memory);
    if (gpu->mpm_label_b_mapped) vkUnmapMemory(gpu->device, gpu->mpm_label_b_memory);
    if (gpu->mpm_component_size_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_component_size_memory);
    DESTROY_MPM_BUFFER(proposal);
    DESTROY_MPM_BUFFER(output);
    DESTROY_MPM_BUFFER(grid);
    if (gpu->mpm_force_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_force_staging_memory);
    if (gpu->mpm_velocity_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_velocity_staging_memory);
    if (gpu->mpm_force_staging_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_force_staging_buffer, NULL);
    if (gpu->mpm_velocity_staging_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_velocity_staging_buffer, NULL);
    if (gpu->mpm_force_staging_memory)
        vkFreeMemory(gpu->device, gpu->mpm_force_staging_memory, NULL);
    if (gpu->mpm_velocity_staging_memory)
        vkFreeMemory(gpu->device, gpu->mpm_velocity_staging_memory, NULL);
    if (gpu->mpm_force_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_force_buffer, NULL);
    if (gpu->mpm_velocity_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_velocity_buffer, NULL);
    if (gpu->mpm_force_memory)
        vkFreeMemory(gpu->device, gpu->mpm_force_memory, NULL);
    if (gpu->mpm_velocity_memory)
        vkFreeMemory(gpu->device, gpu->mpm_velocity_memory, NULL);
    DESTROY_MPM_BUFFER(accept);
    DESTROY_MPM_BUFFER(activity);
    DESTROY_MPM_BUFFER(label_a);
    DESTROY_MPM_BUFFER(label_b);
    DESTROY_MPM_BUFFER(component_size);
}
#undef DESTROY_MPM_BUFFER

bool dc_gpu_mpm_readback_scratch(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "Invalid MPM diagnostic readback");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset MPM diagnostic command");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin MPM diagnostic command");
    VkMemoryBarrier2 before = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &before };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    VkBufferCopy force_copy = { .size = (VkDeviceSize)gpu->width * gpu->height * 16u };
    VkBufferCopy velocity_copy = { .size = (VkDeviceSize)gpu->width * gpu->height * 8u };
    vkCmdCopyBuffer(gpu->command, gpu->mpm_force_buffer,
                    gpu->mpm_force_staging_buffer, 1, &force_copy);
    vkCmdCopyBuffer(gpu->command, gpu->mpm_velocity_buffer,
                    gpu->mpm_velocity_staging_buffer, 1, &velocity_copy);
    VkMemoryBarrier2 after = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT };
    dependency.pMemoryBarriers = &after;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end MPM diagnostic command");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "MPM diagnostic readback failed");
    return true;
}

static void mpm_barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 target_stage,
                        VkAccessFlags2 target_access) {
    VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = target_stage, .dstAccessMask = target_access };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_mpm(dc_gpu_t *gpu) {
    float time_scale = gpu->timed_fluid ? gpu->tick_time_scale : 1.0f;
    float whole_ticks = (float)(uint32_t)(time_scale + 0.5f);
    float drift = time_scale - whole_ticks;
    if (drift > -0.0001f && drift < 0.0001f) time_scale = whole_ticks;
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_activity_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0u, 0u, 0u,
                         dc_gpu_float_bits(time_scale), 0u };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
                  (gpu->height + 15u) / 16u, 1u);
    mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    push[2] = 1u;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1u, 1u, 1u);
    mpm_barrier(gpu, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_component_pipeline);
    VkDeviceSize component_indirect = (VkDeviceSize)(4u + 2u *
        ((gpu->width + 15u) / 16u) * ((gpu->height + 15u) / 16u)) * sizeof(uint32_t);
    uint32_t rounds = component_round_limit(gpu->width, gpu->height);
    for (uint32_t round = 0; round < rounds + 2u; ++round) {
        uint32_t first = round == 0u ? 0u :
                         round == rounds + 1u ? 4u : 1u;
        uint32_t last = round == 0u ? 0u :
                        round == rounds + 1u ? 4u : 3u;
        for (uint32_t mode = first; mode <= last; ++mode) {
            push[2] = mode;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                                  mode >= 1u && mode <= 3u ? component_indirect : 0);
            mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }
        if (round > 0u && round <= rounds) {
            push[2] = 5u;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatch(gpu->command, 1u, 1u, 1u);
            mpm_barrier(gpu, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        }
    }
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_pipeline);
    push[2] = DC_MPM_MOISTURE_MODE;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
    mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t substeps = (uint32_t)ceilf(time_scale * DC_MPM_SUBSTEPS);
    if (substeps < DC_MPM_SUBSTEPS) substeps = DC_MPM_SUBSTEPS;
    push[5] = dc_gpu_float_bits(time_scale * DC_MPM_SUBSTEPS /
                                (float)substeps);
    for (uint32_t step = 0; step < substeps; ++step) {
        for (uint32_t mode = 0; mode < DC_MPM_MODES; ++mode) {
            push[2] = mode;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
            mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            if (mode == 0u) {
                push[2] = 8u;
                vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
                vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
                mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            }
            if (mode == 1u) {
                push[2] = DC_MPM_WATER_FEEDBACK_MODE;
                vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                    VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
                vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
                mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            }
        }
    }
}
