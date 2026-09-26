#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_halo_cell_t) == 16, "GPU halo layout must match SPIR-V");
_Static_assert(sizeof(dc_gpu_transfer_t) == 40, "GPU transfer layout must match SPIR-V");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_halo_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize bytes = (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_GPU_HALO_CELLS *
                         sizeof(dc_gpu_halo_cell_t);
    return dc_gpu_make_mapped_buffer(gpu, bytes, &gpu->halo_buffer,
               &gpu->halo_memory, &gpu->halo_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, sizeof(dc_gpu_transfer_t),
               &gpu->transfer_buffer, &gpu->transfer_memory,
               &gpu->transfer_mapped, err, cap);
}

bool dc_gpu_halo_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/halo.comp.spv",
                                   &shader, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                                1, &info, NULL, &gpu->halo_pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create GPU halo pipeline");
    return true;
}

void dc_gpu_halo_destroy(dc_gpu_t *gpu) {
    if (gpu->halo_pipeline) vkDestroyPipeline(gpu->device, gpu->halo_pipeline, NULL);
    if (gpu->halo_mapped) vkUnmapMemory(gpu->device, gpu->halo_memory);
    if (gpu->transfer_mapped) vkUnmapMemory(gpu->device, gpu->transfer_memory);
    if (gpu->halo_buffer) vkDestroyBuffer(gpu->device, gpu->halo_buffer, NULL);
    if (gpu->transfer_buffer) vkDestroyBuffer(gpu->device, gpu->transfer_buffer, NULL);
    if (gpu->halo_memory) vkFreeMemory(gpu->device, gpu->halo_memory, NULL);
    if (gpu->transfer_memory) vkFreeMemory(gpu->device, gpu->transfer_memory, NULL);
}

static void barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 source_stage,
                    VkAccessFlags2 source_access, VkPipelineStageFlags2 target_stage,
                    VkAccessFlags2 target_access) {
    VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = source_stage, .srcAccessMask = source_access,
        .dstStageMask = target_stage, .dstAccessMask = target_access };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

static bool submit_halos(dc_gpu_t *gpu, bool resolve, char *err, uint32_t cap) {
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset halo command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin halo command buffer");
    barrier(gpu, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->halo_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0, 0, 0, 0, 0 };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    const dc_gpu_transfer_t *queued = gpu->transfer_mapped;
    if (!resolve || !queued->direct_slots)
        vkCmdDispatch(gpu->command, (DC_GPU_HALO_SIDE + 7) / 8,
                      (DC_GPU_HALO_SIDE + 7) / 8,
                      gpu->page_width * gpu->page_height);
    if (resolve) {
        barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        push[2] = 1;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, 1, 1, 1);
    }
    barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_HOST_READ_BIT);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end halo command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU halo submission failed");
    return true;
}

bool dc_gpu_refresh_halos(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    return submit_halos(gpu, false, err, cap);
}

bool dc_gpu_read_halo(dc_gpu_t *gpu, uint32_t tile_x, uint32_t tile_y,
                      int32_t local_x, int32_t local_y, dc_gpu_halo_cell_t *cell,
                      char *err, uint32_t cap) {
    if (!gpu || !cell || tile_x >= gpu->page_width || tile_y >= gpu->page_height ||
        local_x < -1 || local_x > (int32_t)DC_CHUNK_SIDE ||
        local_y < -1 || local_y > (int32_t)DC_CHUNK_SIDE)
        return error(err, cap, "Invalid GPU halo readback");
    const uint32_t *pages = gpu->page_mapped;
    uint32_t page = pages[tile_y * gpu->page_width + tile_x];
    if (!page) return error(err, cap, "GPU halo tile is not resident");
    size_t offset = (size_t)(page - 1u) * DC_GPU_HALO_CELLS +
                    (size_t)(local_y + 1) * DC_GPU_HALO_SIDE + (size_t)(local_x + 1);
    const dc_gpu_halo_cell_t *halos = gpu->halo_mapped;
    *cell = halos[offset];
    return true;
}

bool dc_gpu_queue_transfer(dc_gpu_t *gpu, dc_gpu_transfer_t transfer,
                           char *err, uint32_t cap) {
    if (!gpu || transfer.from_x >= gpu->width || transfer.to_x >= gpu->width ||
        transfer.from_y >= gpu->height || transfer.to_y >= gpu->height ||
        (transfer.kind != DC_GPU_TRANSFER_SCALAR &&
         transfer.kind != DC_GPU_TRANSFER_PARTICLE) ||
        (transfer.kind == DC_GPU_TRANSFER_SCALAR &&
         (!transfer.amount || transfer.amount > DC_FLUID_FULL)))
        return error(err, cap, "Invalid GPU transfer");
    uint32_t dx = transfer.from_x > transfer.to_x ?
        transfer.from_x - transfer.to_x : transfer.to_x - transfer.from_x;
    uint32_t dy = transfer.from_y > transfer.to_y ?
        transfer.from_y - transfer.to_y : transfer.to_y - transfer.from_y;
    if (dx + dy != 1)
        return error(err, cap, "GPU transfer cells must be adjacent");
    dc_gpu_transfer_t *queued = gpu->transfer_mapped;
    if (gpu->has_transfer && queued->state == DC_GPU_TRANSFER_PENDING)
        return error(err, cap, "Previous GPU transfer is still pending");
    transfer.state = DC_GPU_TRANSFER_PENDING;
    transfer.direct_slots = 0;
    memcpy(queued, &transfer, sizeof(transfer));
    gpu->has_transfer = true;
    return true;
}

bool dc_gpu_queue_slot_transfer(dc_gpu_t *gpu, dc_gpu_transfer_t transfer,
                                char *err, uint32_t cap) {
    if (!gpu || transfer.from_slot >= DC_GPU_CHUNK_SLOTS ||
        transfer.to_slot >= DC_GPU_CHUNK_SLOTS ||
        transfer.from_x >= DC_CHUNK_SIDE || transfer.from_y >= DC_CHUNK_SIDE ||
        transfer.to_x >= DC_CHUNK_SIDE || transfer.to_y >= DC_CHUNK_SIDE ||
        (transfer.kind != DC_GPU_TRANSFER_SCALAR &&
         transfer.kind != DC_GPU_TRANSFER_PARTICLE) ||
        (transfer.kind == DC_GPU_TRANSFER_SCALAR &&
         (!transfer.amount || transfer.amount > DC_FLUID_FULL)))
        return error(err, cap, "Invalid slot transfer");
    dc_gpu_transfer_t *queued = gpu->transfer_mapped;
    if (gpu->has_transfer && queued->state == DC_GPU_TRANSFER_PENDING)
        return error(err, cap, "Previous GPU transfer is still pending");
    transfer.state = DC_GPU_TRANSFER_PENDING;
    transfer.direct_slots = 1;
    memcpy(queued, &transfer, sizeof(transfer));
    gpu->has_transfer = true;
    return true;
}

bool dc_gpu_try_transfer(dc_gpu_t *gpu, dc_gpu_transfer_state_t *state,
                         char *err, uint32_t cap) {
    if (!gpu || !state || !gpu->has_transfer)
        return error(err, cap, "No GPU transfer is queued");
    if (!submit_halos(gpu, true, err, cap)) return false;
    const dc_gpu_transfer_t *queued = gpu->transfer_mapped;
    *state = (dc_gpu_transfer_state_t)queued->state;
    return true;
}
