#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_body_t) == 32, "GPU body layout must match SPIR-V");
_Static_assert(sizeof(dc_gpu_body_record_t) == 48, "GPU body record must match SPIR-V");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static uint32_t body_slot(const dc_gpu_t *gpu, uint32_t id) {
    for (uint32_t i = 0; i < gpu->body_count; ++i)
        if (gpu->body_ids[i] == id) return i;
    return DC_GPU_BODY_CAPACITY;
}

bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return dc_gpu_make_mapped_buffer(gpu, sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY,
               &gpu->body_buffer, &gpu->body_memory, &gpu->body_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu,
               (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t) * 2,
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
        body.width > 16 || body.height > 16 || body.width > gpu->width ||
        body.height > gpu->height || body.x_fp < 0 || body.y_fp < 0 ||
        (uint32_t)(body.x_fp >> 16) > gpu->width - body.width ||
        (uint32_t)(body.y_fp >> 16) > gpu->height - body.height)
        return error(err, cap, "Invalid GPU rigid body");
    uint32_t slot = DC_GPU_BODY_CAPACITY;
    for (uint32_t i = 0; i < DC_GPU_BODY_CAPACITY; ++i) {
        if (gpu->body_ids[i] == body.id) { slot = i; break; }
        if (!gpu->body_ids[i] && slot == DC_GPU_BODY_CAPACITY) slot = i;
    }
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body pool is full");
    dc_gpu_body_record_t *records = gpu->body_mapped;
    records[slot] = (dc_gpu_body_record_t){ .body = body,
        .previous_x_fp = body.x_fp, .previous_y_fp = body.y_fp };
    gpu->body_ids[slot] = body.id;
    if (gpu->body_count <= slot) gpu->body_count = slot + 1;
    return true;
}

bool dc_gpu_read_body(dc_gpu_t *gpu, dc_gpu_body_t *body,
                      char *err, uint32_t cap) {
    if (!gpu || !body) return error(err, cap, "Invalid GPU rigid readback");
    memset(body, 0, sizeof(*body));
    for (uint32_t i = 0; i < gpu->body_count; ++i) {
        if (gpu->body_ids[i]) {
            *body = ((dc_gpu_body_record_t *)gpu->body_mapped)[i].body;
            break;
        }
    }
    return true;
}

bool dc_gpu_read_body_id(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_t *body,
                         char *err, uint32_t cap) {
    if (!gpu || !body || !id)
        return error(err, cap, "Invalid GPU body ID readback");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    *body = ((dc_gpu_body_record_t *)gpu->body_mapped)[slot].body;
    return true;
}

bool dc_gpu_remove_body(dc_gpu_t *gpu, uint32_t id, char *err, uint32_t cap) {
    if (!gpu || !id) return error(err, cap, "Invalid GPU body removal");
    uint32_t slot = body_slot(gpu, id);
    if (slot == DC_GPU_BODY_CAPACITY) return error(err, cap, "GPU body ID not found");
    memset(&((dc_gpu_body_record_t *)gpu->body_mapped)[slot], 0,
           sizeof(dc_gpu_body_record_t));
    gpu->body_ids[slot] = 0;
    while (gpu->body_count && !gpu->body_ids[gpu->body_count - 1]) --gpu->body_count;
    return true;
}

bool dc_gpu_read_occupancy(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t *current, uint32_t *swept,
                           char *err, uint32_t cap) {
    if (!gpu || x >= gpu->width || y >= gpu->height || (!current && !swept))
        return error(err, cap, "Invalid GPU occupancy readback");
    const uint32_t *cells = gpu->occupancy_mapped;
    uint32_t value = cells[(size_t)y * gpu->width + x];
    if (current) *current = value;
    if (swept) *swept = cells[(size_t)gpu->width * gpu->height + (size_t)y * gpu->width + x];
    return true;
}

bool dc_gpu_set_body_origin(dc_gpu_t *gpu, dc_chunk_coord_t origin,
                             char *err, uint32_t cap) {
    (void)origin;
    return gpu ? true : error(err, cap, "Invalid body origin");
}

bool dc_gpu_spawn_world_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                              char *err, uint32_t cap) {
    return dc_gpu_spawn_body(gpu, body.body, err, cap);
}

bool dc_gpu_read_world_body(dc_gpu_t *gpu, uint32_t id, dc_gpu_world_body_t *body,
                             char *err, uint32_t cap) {
    if (!body) return error(err, cap, "Invalid world body readback");
    dc_gpu_body_t local;
    if (!dc_gpu_read_body_id(gpu, id, &local, err, cap)) return false;
    *body = (dc_gpu_world_body_t){ .body = local };
    return true;
}

bool dc_gpu_save_bodies(dc_gpu_t *gpu, const char *path, char *err, uint32_t cap) {
    (void)gpu; (void)path;
    return error(err, cap, "World body snapshots are not implemented");
}

bool dc_gpu_load_bodies(dc_gpu_t *gpu, const char *path, char *err, uint32_t cap) {
    (void)gpu; (void)path;
    return error(err, cap, "World body snapshots are not implemented");
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
    if (!gpu->body_count && !gpu->rigid_occupancy_present) return;
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
    uint32_t push[7] = { gpu->width, gpu->height, 0, gpu->body_count, 0, 0, 0 };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    if (gpu->body_count) vkCmdDispatch(gpu->command, 1, 1, 1);
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
    gpu->rigid_occupancy_present = gpu->body_count != 0;
}
