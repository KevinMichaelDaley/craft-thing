#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_body_t) == 32, "GPU body layout must match SPIR-V");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return dc_gpu_make_mapped_buffer(gpu, sizeof(dc_gpu_body_t),
               &gpu->body_buffer, &gpu->body_memory, &gpu->body_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu,
               (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t),
               &gpu->occupancy_buffer, &gpu->occupancy_memory,
               &gpu->occupancy_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, 3 * sizeof(uint32_t),
               &gpu->trace_buffer, &gpu->trace_memory,
               &gpu->trace_mapped, err, cap);
}

bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *path,
                                char *err, uint32_t cap) {
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &shader, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
        &info, NULL, &gpu->rigid_pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create rigid pipeline");
    return true;
}

void dc_gpu_rigid_destroy(dc_gpu_t *gpu) {
    dc_gpu_tick_destroy(gpu);
    if (gpu->rigid_pipeline) vkDestroyPipeline(gpu->device, gpu->rigid_pipeline, NULL);
    if (gpu->body_mapped) vkUnmapMemory(gpu->device, gpu->body_memory);
    if (gpu->occupancy_mapped) vkUnmapMemory(gpu->device, gpu->occupancy_memory);
    if (gpu->body_buffer) vkDestroyBuffer(gpu->device, gpu->body_buffer, NULL);
    if (gpu->occupancy_buffer) vkDestroyBuffer(gpu->device, gpu->occupancy_buffer, NULL);
    if (gpu->body_memory) vkFreeMemory(gpu->device, gpu->body_memory, NULL);
    if (gpu->occupancy_memory) vkFreeMemory(gpu->device, gpu->occupancy_memory, NULL);
}

bool dc_gpu_spawn_body(dc_gpu_t *gpu, dc_gpu_body_t body,
                       char *err, uint32_t cap) {
    if (!gpu || !body.active || !body.id || !body.width || !body.height ||
        body.width > 16 || body.height > 16)
        return error(err, cap, "Invalid GPU rigid body");
    memcpy(gpu->body_mapped, &body, sizeof(body));
    return true;
}

bool dc_gpu_read_body(dc_gpu_t *gpu, dc_gpu_body_t *body,
                      char *err, uint32_t cap) {
    if (!gpu || !body) return error(err, cap, "Invalid GPU rigid readback");
    memcpy(body, gpu->body_mapped, sizeof(*body));
    return true;
}

bool dc_gpu_rigid_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset rigid command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin rigid command buffer");
    dc_gpu_record_rigid(gpu);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end rigid command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU rigid submission failed");
    return true;
}

void dc_gpu_record_rigid(dc_gpu_t *gpu) {
    VkMemoryBarrier2 upload = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dep = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &upload };
    vkCmdPipelineBarrier2(gpu->command, &dep);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->rigid_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0, 0, 0, 0, 0 };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1, 1, 1);
    VkMemoryBarrier2 between = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    dep.pMemoryBarriers = &between;
    vkCmdPipelineBarrier2(gpu->command, &dep);
    push[2] = 1;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u,
        (gpu->height + 15u) / 16u, 1);
    VkMemoryBarrier2 finish = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT };
    dep.pMemoryBarriers = &finish;
    vkCmdPipelineBarrier2(gpu->command, &dep);
}
