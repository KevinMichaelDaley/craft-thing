#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "body_address_gpu.h"
#include "gas_gpu_internal.h"
#include "gpu_internal.h"
#include "renderer_internal.h"
#include "software_present.h"
#include "runtime_body_gpu.h"
#include "thermal_gpu_internal.h"

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

enum { RETICLE_ARM_COUNT = 4, RETICLE_EDGE = 7, RETICLE_GAP = 2 };

static void record_reticles(dc_gpu_t *gpu, VkImage destination,
                            const dc_presentation_layout_t *layouts) {
    static const int32_t arms[RETICLE_ARM_COUNT][4] = {
        {-RETICLE_EDGE, -1, -RETICLE_GAP, 1},
        {RETICLE_GAP, -1, RETICLE_EDGE, 1},
        {-1, -RETICLE_EDGE, 1, -RETICLE_GAP},
        {-1, RETICLE_GAP, 1, RETICLE_EDGE}
    };
    VkImageBlit blits[DC_PRESENTATION_VIEW_LIMIT * RETICLE_ARM_COUNT];
    uint32_t count = 0u;
    for (uint32_t n = 0; n < gpu->presentation_split_count; ++n) {
        const dc_presentation_layout_t *view = &layouts[n];
        dc_presentation_reticle_t aim = gpu->presentation_reticles[n];
        if (!aim.visible || aim.x < view->x || aim.x - view->x >= view->width ||
            aim.y < view->y || aim.y - view->y >= view->height) continue;
        int32_t left = (int32_t)view->left, top = (int32_t)view->top;
        int32_t right = left + (int32_t)(view->width * view->scale);
        int32_t bottom = top + (int32_t)(view->height * view->scale);
        int32_t x = left + (int32_t)((aim.x - view->x) * view->scale + view->scale / 2u);
        int32_t y = top + (int32_t)((aim.y - view->y) * view->scale + view->scale / 2u);
        for (uint32_t arm = 0; arm < RETICLE_ARM_COUNT; ++arm) {
            int32_t x0 = x + arms[arm][0], y0 = y + arms[arm][1];
            int32_t x1 = x + arms[arm][2], y1 = y + arms[arm][3];
            if (x0 < left) x0 = left;
            if (y0 < top) y0 = top;
            if (x1 > right) x1 = right;
            if (y1 > bottom) y1 = bottom;
            if (x0 >= x1 || y0 >= y1) continue;
            blits[count++] = (VkImageBlit){
                .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .srcOffsets = {{0, 0, 0}, {1, 1, 1}},
                .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .dstOffsets = {{x0, y0, 0}, {x1, y1, 1}}
            };
        }
    }
    if (!count) return;
    const VkClearColorValue white = {.float32 = {1.f, 1.f, 1.f, 1.f}};
    const VkImageSubresourceRange color = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    const VkPipelineStageFlags2 transfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    const VkAccessFlags2 write = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    image_barrier(gpu->command, gpu->reticle_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0, transfer, write);
    vkCmdClearColorImage(gpu->command, gpu->reticle_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1, &color);
    image_barrier(gpu->command, gpu->reticle_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer, write, transfer, VK_ACCESS_2_TRANSFER_READ_BIT);
    image_barrier(gpu->command, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, transfer, write, transfer, write);
    vkCmdBlitImage(gpu->command, gpu->reticle_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, count, blits, VK_FILTER_NEAREST);
}

static void record_composed_readback(dc_gpu_t *gpu, VkImage composed, VkBuffer capture) {
    const VkPipelineStageFlags2 transfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    image_barrier(gpu->command, composed, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        transfer, VK_ACCESS_2_TRANSFER_READ_BIT);
    const VkBufferImageCopy readback = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {gpu->swap_extent.width, gpu->swap_extent.height, 1},
    };
    vkCmdCopyImageToBuffer(gpu->command, composed, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        capture, 1, &readback);
    fr_renderer_barrier(gpu->command, transfer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
}

static bool present_frame(dc_gpu_t *gpu, bool render_chunks, uint32_t steps, dc_gpu_address_buffer_t *capture,
                          uint32_t *completed, char *err, uint32_t cap) {
    if (completed) *completed = 0;
    if (!gpu || !gpu->swapchain) return error(err, cap, "GPU window is not initialized");
    if (dc_gpu_liquid_seam_pending(gpu))
        return error(err, cap, "Cannot present an owner with an unresolved liquid seam");
    if (steps && gpu->water_cycle_peer)
        return error(err, cap, "Cannot advance a single owner during a coupled water cycle");
    const char *trace_setting=getenv("DC_FRAME_TIMINGS");
    bool trace_waits=trace_setting&&strcmp(trace_setting,"0")!=0;
    uint64_t setup_begin=trace_waits?SDL_GetPerformanceCounter():0;
    if(gpu->gas) {
        for (uint32_t i = 0; i < steps; ++i) {
            if (!dc_gpu_tick_step(gpu, err, cap)) return false;
            if (completed) ++*completed;
        }
        steps=0;
    }
    uint64_t tick_done=trace_waits?SDL_GetPerformanceCounter():0;
    if(!dc_gpu_runtime_bodies_service(gpu,err,cap)) return false;
    uint64_t service_done=trace_waits?SDL_GetPerformanceCounter():0;
    SDL_PumpEvents();
    uint64_t acquire_begin=trace_waits?SDL_GetPerformanceCounter():0;
    uint32_t index = 0;
    dc_software_present_t *software = gpu->software_present;
    VkResult result = vkAcquireNextImageKHR(software ? software->device : gpu->device,
        gpu->swapchain, ACQUIRE_TIMEOUT_NS, software ? software->acquire : gpu->acquire_sem,
        VK_NULL_HANDLE, &index);
    uint64_t acquire_done=trace_waits?SDL_GetPerformanceCounter():0;
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
        return error(err, cap, "Vulkan window image acquisition timed out");
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Cannot acquire Vulkan window image");
    const VkImage composed = software ? software->composed : gpu->swap_images[index];
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
        fr_renderer_record_fire_coverage_reset(gpu);
        if (fr_renderer_active(gpu)) {
            fr_renderer_record(gpu);
        } else {
            uint32_t push[7] = { gpu->width, gpu->height, 2,
                                 dc_gpu_render_flags(gpu), (uint32_t)gpu->overlay,
                                 gpu->view_x, gpu->view_y };
            dc_gpu_record_draw(gpu, push);
        }
        fr_gas_record_overlay(gpu);
        fr_thermal_record_overlay(gpu);
        fr_renderer_record_fire_coverage_readback(gpu);
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
    image_barrier(gpu->command, composed, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    dc_presentation_layout_t layouts[DC_PRESENTATION_VIEW_LIMIT];
    uint32_t band_count = gpu->presentation_split_count ? gpu->presentation_split_count : 1u;
    for (uint32_t n = 0; n < band_count; ++n)
        if (!dc_gpu_presentation_view(gpu,n,&layouts[n]))
            return error(err,cap,"Invalid presentation crop");
    uint32_t scaled_width=layouts[0].width*layouts[0].scale,
             scaled_height=layouts[0].height*layouts[0].scale;
    if (scaled_width != gpu->swap_extent.width ||
        scaled_height != gpu->swap_extent.height) {
        VkClearColorValue background = { .float32 = {0.02f, 0.02f, 0.02f, 1.0f} };
        VkImageSubresourceRange color_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(gpu->command, composed,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &background, 1, &color_range);
        image_barrier(gpu->command, composed,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    }
    for (uint32_t band = 0u; band < band_count; band++) {
        const dc_presentation_layout_t *layout = &layouts[band];
        uint32_t band_scaled_width = layout->width * layout->scale,
                 band_scaled_height = layout->height * layout->scale;
        uint32_t left = layout->left, top = layout->top,
                 source_x = gpu->view_x + layout->x,
                 source_y = gpu->view_y + layout->y;
        VkImageBlit blit = { .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .srcOffsets = { {(int32_t)source_x, (int32_t)source_y, 0},
                            {(int32_t)(source_x + layout->width),
                             (int32_t)(source_y + layout->height), 1} },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .dstOffsets = { {(int32_t)left, (int32_t)top, 0},
                            {(int32_t)(left + band_scaled_width),
                             (int32_t)(top + band_scaled_height), 1} } };
        vkCmdBlitImage(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            composed, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        if (band + 1u < band_count) {
            image_barrier(gpu->command, composed,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        }
    }
    record_reticles(gpu, composed, layouts);
    if (software) record_composed_readback(gpu, composed, software->readback.buffer);
    else {
        if (capture) record_composed_readback(gpu, composed, capture->buffer);
        image_barrier(gpu->command, composed,
            capture ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            capture ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
    }
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end present command buffer");
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = software ? 0u : 1u, .pWaitSemaphores = &gpu->acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command,
        .signalSemaphoreCount = software ? 0u : 1u, .pSignalSemaphores = software ? NULL : &gpu->present_sems[index] };
    if (vkResetFences(gpu->device, 1, &gpu->render_fence) != VK_SUCCESS)
        return error(err, cap, "Cannot reset presentation rendering fence");
    if (vkQueueSubmit(gpu->queue, 1, &submit, gpu->render_fence) != VK_SUCCESS)
        return error(err, cap, "Vulkan present submission failed");
    uint64_t present_begin=trace_waits?SDL_GetPerformanceCounter():0;
    if (!software) {
        VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->present_sems[index],
            .swapchainCount = 1, .pSwapchains = &gpu->swapchain, .pImageIndices = &index };
        result = vkQueuePresentKHR(gpu->queue, &present);
    }
    uint64_t wait_begin=trace_waits?SDL_GetPerformanceCounter():0;
    uint64_t present_ticks=wait_begin-present_begin;
    /* The render fence also commits any ticks recorded in the frame command. */
    if (vkWaitForFences(gpu->device, 1, &gpu->render_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        return error(err, cap, "Vulkan present wait failed");
    uint64_t fence_done=trace_waits?SDL_GetPerformanceCounter():0;
    if (completed) *completed += steps;
    if (software) {
        result = dc_software_present_frame(gpu, index, capture ? capture->mapped : NULL);
        if (trace_waits) present_ticks=SDL_GetPerformanceCounter()-fence_done;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Vulkan swapchain presentation failed");
    if (render_chunks) fr_renderer_fire_coverage_presented(gpu);
    if(trace_waits) {
        double scale=1000./SDL_GetPerformanceFrequency();
        fprintf(stderr,"Present setup tick%.3f service%.3f pump%.3f\n",(tick_done-setup_begin)*scale,
            (service_done-tick_done)*scale,(acquire_begin-service_done)*scale);
        fprintf(stderr,"Present waits acquire%.3f fence%.3f present%.3f\n",(acquire_done-acquire_begin)*scale,
            (fence_done-wait_begin)*scale,present_ticks*scale);
    }
    return true;
}

bool dc_gpu_present(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return present_frame(gpu, false, 0, VK_NULL_HANDLE, NULL, err, cap);
}

bool dc_gpu_present_chunks(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return present_frame(gpu, true, 0, VK_NULL_HANDLE, NULL, err, cap);
}

bool dc_gpu_present_chunks_steps(dc_gpu_t *gpu, uint32_t steps,
                                 char *err, uint32_t cap) {
    return dc_gpu_present_chunks_steps_progress(gpu, steps, NULL, err, cap);
}

bool dc_gpu_present_chunks_steps_progress(dc_gpu_t *gpu, uint32_t steps,
                                          uint32_t *completed,
                                          char *err, uint32_t cap) {
    return present_frame(gpu, true, steps, VK_NULL_HANDLE, completed, err, cap);
}

bool dc_gpu_capture_present(dc_gpu_t *gpu, uint32_t *pixels, size_t count,
                            char *err, uint32_t cap) {
    if (!gpu || !gpu->swapchain || !pixels)
        return error(err, cap, "Window capture requires a Vulkan window and pixel buffer");
    uint64_t pixel_count = (uint64_t)gpu->swap_extent.width * gpu->swap_extent.height;
    if (!pixel_count || pixel_count > SIZE_MAX / sizeof(*pixels) || count < pixel_count)
        return error(err, cap, "Window capture pixel buffer is too small");
    bool bgra = gpu->swap_format == VK_FORMAT_B8G8R8A8_UNORM ||
                gpu->swap_format == VK_FORMAT_B8G8R8A8_SRGB;
    if (!gpu->swap_transfer_src || (!bgra && gpu->swap_format != VK_FORMAT_R8G8B8A8_UNORM &&
                                           gpu->swap_format != VK_FORMAT_R8G8B8A8_SRGB))
        return error(err, cap, "Window format cannot be captured as RGBA8");

    dc_gpu_address_buffer_t staging = {0};
    if (!dc_gpu_address_buffer_create(gpu, pixel_count * sizeof(*pixels), true, &staging, err, cap))
        return false;
    bool okay = present_frame(gpu, false, 0, &staging, NULL, err, cap);
    if (okay) {
        const uint8_t *sample = staging.mapped;
        for (size_t i = 0; i < (size_t)pixel_count; ++i, sample += 4u) {
            uint32_t red = sample[bgra ? 2u : 0u], blue = sample[bgra ? 0u : 2u];
            pixels[i] = red | ((uint32_t)sample[1] << 8u) | (blue << 16u) |
                        ((uint32_t)sample[3] << 24u);
        }
    }
    dc_gpu_address_buffer_destroy(gpu, &staging);
    return okay;
}
