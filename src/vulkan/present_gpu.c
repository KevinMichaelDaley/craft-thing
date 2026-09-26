#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static void image_barrier(VkCommandBuffer command, VkImage image,
                          VkImageLayout old_layout, VkImageLayout new_layout,
                          VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                          VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage, .srcAccessMask = src_access,
        .dstStageMask = dst_stage, .dstAccessMask = dst_access,
        .oldLayout = old_layout, .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(command, &dependency);
}

static bool present_frame(dc_gpu_t *gpu, bool render_chunks, uint32_t steps,
                          char *err, uint32_t cap) {
    if (!gpu || !gpu->swapchain) return error(err, cap, "GPU window is not initialized");
    uint32_t index = 0;
    VkResult result = vkAcquireNextImageKHR(gpu->device, gpu->swapchain, UINT64_MAX,
        gpu->acquire_sem, VK_NULL_HANDLE, &index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Cannot acquire Vulkan window image");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset present command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin present command buffer");
    for (uint32_t i = 0; i < steps; ++i) dc_gpu_record_tick_step(gpu);
    if (render_chunks) {
        VkMemoryBarrier2 upload = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT };
        VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &upload };
        vkCmdPipelineBarrier2(gpu->command, &dependency);
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->pipeline);
        vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
            gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
        uint32_t push[7] = { gpu->width, gpu->height, 2,
                             gpu->marker_overlay ? 1u : 0u, 0,
                             gpu->view_x, gpu->view_y };
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, (gpu->view_width + 15u) / 16u,
                      (gpu->view_height + 15u) / 16u, 1);
    }
    VkBufferMemoryBarrier2 buffer_barrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = gpu->cells, .offset = 0, .size = VK_WHOLE_SIZE };
    VkDependencyInfo buffer_dep = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &buffer_barrier };
    vkCmdPipelineBarrier2(gpu->command, &buffer_dep);
    image_barrier(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy = {
        .bufferOffset = ((VkDeviceSize)gpu->view_y * gpu->width + gpu->view_x) * 4u,
        .bufferRowLength = gpu->width,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = { (int32_t)gpu->view_x, (int32_t)gpu->view_y, 0 },
        .imageExtent = { gpu->view_width, gpu->view_height, 1 } };
    vkCmdCopyBufferToImage(gpu->command, gpu->cells, gpu->frame_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    image_barrier(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT);
    image_barrier(gpu->command, gpu->swap_images[index], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkImageBlit blit = { .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { {(int32_t)gpu->view_x, (int32_t)gpu->view_y, 0},
                        {(int32_t)(gpu->view_x + gpu->view_width),
                         (int32_t)(gpu->view_y + gpu->view_height), 1} },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { {0, 0, 0}, {(int32_t)gpu->swap_extent.width, (int32_t)gpu->swap_extent.height, 1} } };
    vkCmdBlitImage(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    image_barrier(gpu->command, gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end present command buffer");
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &gpu->present_sem };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        return error(err, cap, "Vulkan present submission failed");
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->present_sem,
        .swapchainCount = 1, .pSwapchains = &gpu->swapchain, .pImageIndices = &index };
    result = vkQueuePresentKHR(gpu->queue, &present);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Vulkan swapchain presentation failed");
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Vulkan present wait failed");
    return true;
}

bool dc_gpu_present(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return present_frame(gpu, false, 0, err, cap);
}

bool dc_gpu_present_chunks(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return present_frame(gpu, true, 0, err, cap);
}

bool dc_gpu_present_chunks_steps(dc_gpu_t *gpu, uint32_t steps,
                                 char *err, uint32_t cap) {
    return present_frame(gpu, true, steps, err, cap);
}
