#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

enum { DC_PRESSURE_SWEEPS = 20 };

bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize bytes = (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t);
    return dc_gpu_make_mapped_buffer(gpu, bytes, &gpu->fluid_a_buffer,
               &gpu->fluid_a_memory, &gpu->fluid_a_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, bytes, &gpu->fluid_b_buffer,
               &gpu->fluid_b_memory, &gpu->fluid_b_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, bytes * 2, &gpu->velocity_buffer,
               &gpu->velocity_memory, &gpu->velocity_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, bytes, &gpu->pressure_a_buffer,
               &gpu->pressure_a_memory, &gpu->pressure_a_mapped, err, cap);
}

static bool make_pipeline(dc_gpu_t *gpu, const char *path, VkPipeline *pipeline,
                          char *err, uint32_t cap) {
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &shader, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                                1, &info, NULL, pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create Eulerian fluid pipeline");
    return true;
}

bool dc_gpu_fluid_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return make_pipeline(gpu, "build/shaders/fluid.comp.spv",
                         &gpu->fluid_pipeline, err, cap) &&
           make_pipeline(gpu, "build/shaders/projection.comp.spv",
                         &gpu->projection_pipeline, err, cap);
}

void dc_gpu_fluid_destroy(dc_gpu_t *gpu) {
    if (gpu->fluid_pipeline) vkDestroyPipeline(gpu->device, gpu->fluid_pipeline, NULL);
    if (gpu->projection_pipeline) vkDestroyPipeline(gpu->device, gpu->projection_pipeline, NULL);
    if (gpu->fluid_a_mapped) vkUnmapMemory(gpu->device, gpu->fluid_a_memory);
    if (gpu->fluid_b_mapped) vkUnmapMemory(gpu->device, gpu->fluid_b_memory);
    if (gpu->velocity_mapped) vkUnmapMemory(gpu->device, gpu->velocity_memory);
    if (gpu->pressure_a_mapped) vkUnmapMemory(gpu->device, gpu->pressure_a_memory);
    if (gpu->fluid_a_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_a_buffer, NULL);
    if (gpu->fluid_b_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_b_buffer, NULL);
    if (gpu->velocity_buffer) vkDestroyBuffer(gpu->device, gpu->velocity_buffer, NULL);
    if (gpu->pressure_a_buffer) vkDestroyBuffer(gpu->device, gpu->pressure_a_buffer, NULL);
    if (gpu->fluid_a_memory) vkFreeMemory(gpu->device, gpu->fluid_a_memory, NULL);
    if (gpu->fluid_b_memory) vkFreeMemory(gpu->device, gpu->fluid_b_memory, NULL);
    if (gpu->velocity_memory) vkFreeMemory(gpu->device, gpu->velocity_memory, NULL);
    if (gpu->pressure_a_memory) vkFreeMemory(gpu->device, gpu->pressure_a_memory, NULL);
}

static void fluid_barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 source_stage,
                          VkAccessFlags2 source_access, VkPipelineStageFlags2 target_stage,
                          VkAccessFlags2 target_access) {
    VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = source_stage, .srcAccessMask = source_access,
        .dstStageMask = target_stage, .dstAccessMask = target_access };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_fluid(dc_gpu_t *gpu) {
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0,
                         gpu->fluid_reset_velocity ? 1u : 0u, 0, 0, 0 };
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->projection_pipeline);
    for (uint32_t pass = 0; pass < 2u * DC_PRESSURE_SWEEPS + 2u; ++pass) {
        push[2] = pass == 0 ? 0u : pass == 2u * DC_PRESSURE_SWEEPS + 1u ? 3u :
                  ((pass & 1u) ? 1u : 2u);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
                      (gpu->height + 15u) / 16u, 1);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    gpu->fluid_reset_velocity = false;
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->fluid_pipeline);
    push[3] = gpu->fluid_tick & 1u;
    for (uint32_t mode = 0; mode <= 5; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
                      (gpu->height + 15u) / 16u, 1);
        if (mode < 5)
            fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    ++gpu->fluid_tick;
}

bool dc_gpu_fluid_max_divergence(dc_gpu_t *gpu, float *divergence,
                                 char *err, uint32_t cap) {
    if (!gpu || !divergence) return error(err, cap, "Invalid fluid divergence diagnostic");
    const uint32_t *pages = gpu->page_mapped;
    const dc_cell_t *cells = gpu->chunk_mapped;
    const float *faces = gpu->velocity_mapped;
    float maximum = 0.0f;
    for (uint32_t y = 1; y + 1 < gpu->height; ++y) {
        for (uint32_t x = 1; x + 1 < gpu->width; ++x) {
            uint32_t page = pages[(y / 64u) * gpu->page_width + x / 64u];
            if (!page) continue;
            uint32_t atlas = (page - 1u) * DC_CHUNK_CELLS +
                             (y % 64u) * 64u + x % 64u;
            if (cells[atlas].fluid_mass != DC_FLUID_FULL) continue;
            uint32_t index = y * gpu->width + x;
            float value = faces[2u * index] - faces[2u * (index - 1u)] +
                          faces[2u * index + 1u] -
                          faces[2u * (index - gpu->width) + 1u];
            if (value < 0.0f) value = -value;
            if (value > maximum) maximum = value;
        }
    }
    *divergence = maximum;
    return true;
}

bool dc_gpu_fluid_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset fluid command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin fluid command buffer");
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_fluid(gpu);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_HOST_READ_BIT);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end fluid command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU fluid submission failed");
    return true;
}
