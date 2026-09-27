#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

enum { DC_PRESSURE_SWEEPS = 20 };

bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize bytes = (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t);
    return dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_a_buffer,
               &gpu->fluid_a_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_b_buffer,
               &gpu->fluid_b_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, bytes * 2, &gpu->velocity_buffer,
               &gpu->velocity_memory, &gpu->velocity_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->pressure_a_buffer,
               &gpu->pressure_a_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_previous_buffer,
               &gpu->fluid_previous_memory, err, cap);
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
                         &gpu->projection_pipeline, err, cap) &&
           make_pipeline(gpu, "build/shaders/shift_velocity.comp.spv",
                         &gpu->velocity_shift_pipeline, err, cap);
}

void dc_gpu_fluid_destroy(dc_gpu_t *gpu) {
    if (gpu->fluid_pipeline) vkDestroyPipeline(gpu->device, gpu->fluid_pipeline, NULL);
    if (gpu->projection_pipeline) vkDestroyPipeline(gpu->device, gpu->projection_pipeline, NULL);
    if (gpu->velocity_shift_pipeline)
        vkDestroyPipeline(gpu->device, gpu->velocity_shift_pipeline, NULL);
    if (gpu->fluid_a_mapped) vkUnmapMemory(gpu->device, gpu->fluid_a_memory);
    if (gpu->fluid_b_mapped) vkUnmapMemory(gpu->device, gpu->fluid_b_memory);
    if (gpu->velocity_mapped) vkUnmapMemory(gpu->device, gpu->velocity_memory);
    if (gpu->pressure_a_mapped) vkUnmapMemory(gpu->device, gpu->pressure_a_memory);
    if (gpu->fluid_a_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_a_buffer, NULL);
    if (gpu->fluid_b_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_b_buffer, NULL);
    if (gpu->velocity_buffer) vkDestroyBuffer(gpu->device, gpu->velocity_buffer, NULL);
    if (gpu->pressure_a_buffer) vkDestroyBuffer(gpu->device, gpu->pressure_a_buffer, NULL);
    if (gpu->fluid_previous_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_previous_buffer, NULL);
    if (gpu->fluid_a_memory) vkFreeMemory(gpu->device, gpu->fluid_a_memory, NULL);
    if (gpu->fluid_b_memory) vkFreeMemory(gpu->device, gpu->fluid_b_memory, NULL);
    if (gpu->velocity_memory) vkFreeMemory(gpu->device, gpu->velocity_memory, NULL);
    if (gpu->pressure_a_memory) vkFreeMemory(gpu->device, gpu->pressure_a_memory, NULL);
    if (gpu->fluid_previous_memory) vkFreeMemory(gpu->device, gpu->fluid_previous_memory, NULL);
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

bool dc_gpu_shift_velocity(dc_gpu_t *gpu, int32_t chunk_dx, int32_t chunk_dy,
                           char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "Invalid GPU velocity shift");
    if (!chunk_dx && !chunk_dy) {
        return true;
    }
    for (uint32_t slot = 0; slot < DC_GPU_CHUNK_SLOTS; ++slot)
        gpu->preserve_shifted_slot[slot] = gpu->slot_page[slot] != UINT32_MAX;
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset velocity shift command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin velocity shift command buffer");
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->velocity_shift_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0u,
                         (uint32_t)chunk_dx, (uint32_t)chunk_dy, 0u, 0u };
    for (uint32_t mode = 0; mode < 2u; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command,
            (gpu->width * gpu->height + 255u) / 256u, 1u, 1u);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end velocity shift command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU velocity shift failed");
    return true;
}

static void record_projection_begin(dc_gpu_t *gpu) {
    uint32_t push[7] = { gpu->width, gpu->height, 0, 0, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->projection_pipeline);
    push[2] = 5u;
    push[3] = 1u;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 4u, 4u, DC_GPU_CHUNK_SLOTS);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    push[2] = 4u;
    push[3] = 0u;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u, 1u, 1u);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

static void record_pressure_passes(dc_gpu_t *gpu, uint32_t first,
                                    uint32_t stop) {
    uint32_t push[7] = { gpu->width, gpu->height, 0, 1, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->projection_pipeline);
    for (uint32_t pass = first; pass < stop; ++pass) {
        push[2] = pass == 0 ? 0u : pass == 2u * DC_PRESSURE_SWEEPS + 1u ? 3u :
                  ((pass & 1u) ? 1u : 2u);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, 4u, 4u, DC_GPU_CHUNK_SLOTS);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

static void record_fluid_transport(dc_gpu_t *gpu, uint32_t first,
                                   uint32_t stop) {
    uint32_t push[7] = { gpu->width, gpu->height, 0,
                          gpu->fluid_tick & 1u, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    push[6] = gpu->timed_fluid && gpu->fluid_step_scale > 1.0001f ? 1u : 0u;
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->fluid_pipeline);
    if (first == 0u) {
        push[2] = 10u;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u, 1u, DC_GPU_CHUNK_SLOTS);
        gpu->fluid_snapshot_valid = true;
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    for (uint32_t mode = first; mode < stop; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        uint32_t work = mode < 2u ? gpu->width : gpu->height;
        if (mode < 4u)
            vkCmdDispatch(gpu->command, (work + 63u) / 64u, 1u, 1u);
        else
            vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                          1u, DC_GPU_CHUNK_SLOTS);
        if (mode < 4u)
            fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

static void record_fluid_correction(dc_gpu_t *gpu) {
    dc_gpu_record_markers(gpu);
    uint32_t push[7] = { gpu->width, gpu->height, 0,
                         gpu->fluid_tick & 1u, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    if (gpu->marker_correction) {
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->fluid_pipeline);
        const uint32_t correction_modes[6] = { 4u, 6u, 7u, 8u, 9u, 5u };
        for (uint32_t i = 0; i < 6; ++i) {
            push[2] = correction_modes[i];
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                          1u, DC_GPU_CHUNK_SLOTS);
            if (i < 5)
                fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }
    } else {
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          gpu->fluid_pipeline);
        push[2] = 5u;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                      1u, DC_GPU_CHUNK_SLOTS);
    }
    ++gpu->fluid_tick;
}

void dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase) {
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    if (phase == 0u) record_projection_begin(gpu);
    if (phase == 0u) record_pressure_passes(gpu, 0u, 10u);
    else if (phase == 1u) record_pressure_passes(gpu, 10u, 25u);
    else if (phase == 2u) record_pressure_passes(gpu, 25u, 40u);
    else if (phase == 3u) {
        record_pressure_passes(gpu, 40u, 2u * DC_PRESSURE_SWEEPS + 2u);
        record_fluid_transport(gpu, 0u, 3u);
    } else if (phase == 4u) record_fluid_transport(gpu, 3u, 5u);
    else if (phase == 5u) record_fluid_correction(gpu);
}

void dc_gpu_record_fluid(dc_gpu_t *gpu) {
    for (uint32_t phase = 0; phase < 6u; ++phase)
        dc_gpu_record_fluid_phase(gpu, phase);
}

bool dc_gpu_fluid_max_divergence(dc_gpu_t *gpu, float *divergence,
                                  char *err, uint32_t cap) {
    if (!gpu || !divergence) return error(err, cap, "Invalid fluid divergence diagnostic");
    if (!dc_gpu_copy_chunk_state(gpu, UINT32_MAX, false, false, err, cap)) return false;
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
