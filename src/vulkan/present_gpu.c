#include <stdio.h>

#include "gpu_internal.h"

static const uint64_t ACQUIRE_TIMEOUT_NS = 1000000000ull;

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
    SDL_PumpEvents();
    uint32_t index = 0;
    VkResult result = vkAcquireNextImageKHR(gpu->device, gpu->swapchain, ACQUIRE_TIMEOUT_NS,
        gpu->acquire_sem, VK_NULL_HANDLE, &index);
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
        return error(err, cap, "Vulkan window image acquisition timed out");
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
        if (fr_renderer_active(gpu)) {
            fr_renderer_record(gpu);
        } else {
            uint32_t push[7] = { gpu->width, gpu->height, 2,
                                 dc_gpu_render_flags(gpu), (uint32_t)gpu->overlay,
                                 gpu->view_x, gpu->view_y };
            dc_gpu_record_draw(gpu, push);
        }
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
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    uint32_t scaled_width = gpu->view_width * gpu->display_zoom;
    uint32_t scaled_height = gpu->view_height * gpu->display_zoom;
    uint32_t left = (gpu->swap_extent.width - scaled_width) / 2u;
    uint32_t top = (gpu->swap_extent.height - scaled_height) / 2u;
    if (scaled_width != gpu->swap_extent.width ||
        scaled_height != gpu->swap_extent.height) {
        VkClearColorValue background = { .float32 = {0.02f, 0.02f, 0.02f, 1.0f} };
        VkImageSubresourceRange color_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(gpu->command, gpu->swap_images[index],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &background, 1, &color_range);
        image_barrier(gpu->command, gpu->swap_images[index],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    }
    VkImageBlit blit = { .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { {(int32_t)gpu->view_x, (int32_t)gpu->view_y, 0},
                        {(int32_t)(gpu->view_x + gpu->view_width),
                         (int32_t)(gpu->view_y + gpu->view_height), 1} },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { {(int32_t)left, (int32_t)top, 0},
                        {(int32_t)(left + scaled_width),
                         (int32_t)(top + scaled_height), 1} } };
    vkCmdBlitImage(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    image_barrier(gpu->command, gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end present command buffer");
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &gpu->present_sems[index] };
    if (vkResetFences(gpu->device, 1, &gpu->render_fence) != VK_SUCCESS)
        return error(err, cap, "Cannot reset presentation rendering fence");
    if (vkQueueSubmit(gpu->queue, 1, &submit, gpu->render_fence) != VK_SUCCESS)
        return error(err, cap, "Vulkan present submission failed");
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->present_sems[index],
        .swapchainCount = 1, .pSwapchains = &gpu->swapchain, .pImageIndices = &index };
    result = vkQueuePresentKHR(gpu->queue, &present);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Vulkan swapchain presentation failed");
    /* Wait for command and host-visible physics completion, not compositor display. */
    if (vkWaitForFences(gpu->device, 1, &gpu->render_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
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
