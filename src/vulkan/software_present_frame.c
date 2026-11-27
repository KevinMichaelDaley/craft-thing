#include <string.h>

#include "software_present.h"

static void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                       VkAccessFlags read, VkAccessFlags write, VkPipelineStageFlags dst) {
    VkImageMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    .srcAccessMask = read,
                                    .dstAccessMask = write,
                                    .oldLayout = from,
                                    .newLayout = to,
                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .image = image,
                                    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
}

static VkResult record_upload(dc_software_present_t *s, VkImage image, VkExtent2D extent,
                              bool capture) {
    VkResult result = vkResetCommandBuffer(s->command, 0);
    if (result != VK_SUCCESS) return result;
    VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    result = vkBeginCommandBuffer(s->command, &begin);
    if (result != VK_SUCCESS) return result;
    VkBufferImageCopy copy = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                              .imageExtent = {extent.width, extent.height, 1}};
    transition(s->command, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    vkCmdCopyBufferToImage(s->command, s->upload.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);
    if (capture) {
        transition(s->command, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdCopyImageToBuffer(s->command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               s->upload.buffer, 1, &copy);
        VkMemoryBarrier readback = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                    .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(s->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                             1, &readback, 0, NULL, 0, NULL);
    }
    transition(s->command, image,
               capture ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               capture ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT, 0,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    return vkEndCommandBuffer(s->command);
}

VkResult dc_software_present_frame(dc_gpu_t *gpu, uint32_t index, void *capture) {
    dc_software_present_t *s = gpu->software_present;
    memcpy(s->upload.mapped, s->readback.mapped, (size_t)s->bytes);
    VkResult result = record_upload(s, s->images[index], gpu->swap_extent, capture != NULL);
    if (result != VK_SUCCESS) return result;
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .waitSemaphoreCount = 1,
                           .pWaitSemaphores = &s->acquire,
                           .pWaitDstStageMask = &wait,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &s->command,
                           .signalSemaphoreCount = 1,
                           .pSignalSemaphores = &s->ready[index]};
    result = vkResetFences(s->device, 1, &s->upload_fence);
    if (result != VK_SUCCESS) return result;
    result = vkQueueSubmit(s->queue, 1, &submit, s->upload_fence);
    if (result != VK_SUCCESS) return result;
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1,
                                .pWaitSemaphores = &s->ready[index],
                                .swapchainCount = 1,
                                .pSwapchains = &s->chain,
                                .pImageIndices = &index};
    result = vkQueuePresentKHR(s->queue, &present);
    VkResult complete = vkWaitForFences(s->device, 1, &s->upload_fence, VK_TRUE, UINT64_MAX);
    if (complete != VK_SUCCESS) return complete;
    if (capture && (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR))
        memcpy(capture, s->upload.mapped, (size_t)s->bytes);
    return result;
}
