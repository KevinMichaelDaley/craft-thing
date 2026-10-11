#include <stdio.h>
#include <stdlib.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_window_init(dc_gpu_t *gpu, uint32_t requested_width,
                        uint32_t requested_height, char *err, uint32_t cap) {
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->physical, gpu->surface, &caps) != VK_SUCCESS)
        return error(err, cap, "Cannot query Vulkan surface capabilities");
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return error(err, cap, "Vulkan surface does not support transfer destination images");
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, gpu->surface, &format_count, NULL);
    if (!format_count) return error(err, cap, "Vulkan surface has no formats");
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (!formats) return error(err, cap, "Out of memory listing surface formats");
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, gpu->surface, &format_count, formats);
    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) { chosen = formats[i]; break; }
    }
    free(formats);
    VkFormatProperties frame_props, swap_props;
    vkGetPhysicalDeviceFormatProperties(gpu->physical, VK_FORMAT_R8G8B8A8_UNORM, &frame_props);
    vkGetPhysicalDeviceFormatProperties(gpu->physical, chosen.format, &swap_props);
    if (!(frame_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
        !(swap_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        return error(err, cap, "Vulkan image formats do not support blit presentation");
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = requested_width;
        extent.height = requested_height;
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }
    gpu->swap_extent = extent;
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount && image_count > caps.maxImageCount) image_count = caps.maxImageCount;
    uint32_t mode_count = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->physical, gpu->surface, &mode_count, NULL) != VK_SUCCESS || !mode_count)
        return error(err, cap, "Cannot query Vulkan presentation modes");
    VkPresentModeKHR *modes = calloc(mode_count, sizeof(*modes));
    if (!modes) return error(err, cap, "Out of memory listing presentation modes");
    VkResult mode_result = vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->physical, gpu->surface, &mode_count, modes);
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    /* FIFO can wait indefinitely for a Wayland callback when the window is occluded. */
    for (uint32_t i = 0; mode_result == VK_SUCCESS && i < mode_count; ++i)
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) mode = VK_PRESENT_MODE_MAILBOX_KHR;
    free(modes);
    if (mode_result != VK_SUCCESS) return error(err, cap, "Cannot list Vulkan presentation modes");
    VkSwapchainCreateInfoKHR swap_info = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = gpu->surface, .minImageCount = image_count,
        .imageFormat = chosen.format, .imageColorSpace = chosen.colorSpace,
        .imageExtent = extent, .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = mode, .clipped = VK_TRUE };
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        for (uint32_t bit = 0; bit < 4; ++bit) {
            VkCompositeAlphaFlagBitsKHR alpha = (VkCompositeAlphaFlagBitsKHR)(1u << bit);
            if (caps.supportedCompositeAlpha & alpha) { swap_info.compositeAlpha = alpha; break; }
        }
    }
    if (vkCreateSwapchainKHR(gpu->device, &swap_info, NULL, &gpu->swapchain) != VK_SUCCESS)
        return error(err, cap, "Cannot create Vulkan swapchain");
    vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &gpu->swap_count, NULL);
    gpu->swap_images = calloc(gpu->swap_count, sizeof(*gpu->swap_images));
    if (!gpu->swap_images) return error(err, cap, "Out of memory listing swapchain images");
    vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &gpu->swap_count, gpu->swap_images);
    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { gpu->width, gpu->height, 1 }, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
    if (vkCreateImage(gpu->device, &image_info, NULL, &gpu->frame_image) != VK_SUCCESS)
        return error(err, cap, "Cannot create cell presentation image");
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu->device, gpu->frame_image, &req);
    VkPhysicalDeviceMemoryProperties memory_props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &memory_props);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < memory_props.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (memory_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            type = i; break;
        }
    }
    if (type == UINT32_MAX) return error(err, cap, "No device-local image memory");
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    if (vkAllocateMemory(gpu->device, &alloc, NULL, &gpu->frame_memory) != VK_SUCCESS ||
        vkBindImageMemory(gpu->device, gpu->frame_image, gpu->frame_memory, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate presentation image memory");
    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkCreateSemaphore(gpu->device, &semaphore_info, NULL, &gpu->acquire_sem) != VK_SUCCESS)
        return error(err, cap, "Cannot create present semaphores");
    gpu->present_sems = calloc(gpu->swap_count, sizeof(*gpu->present_sems));
    /* Reacquiring an image orders reuse of its prior presentation wait semaphore. */
    if (!gpu->present_sems) return error(err, cap, "Out of memory allocating presentation semaphores");
    for (uint32_t i = 0; i < gpu->swap_count; ++i)
        if (vkCreateSemaphore(gpu->device, &semaphore_info, NULL, &gpu->present_sems[i]) != VK_SUCCESS)
            return error(err, cap, "Cannot create image presentation semaphore");
    VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (vkCreateFence(gpu->device, &fence_info, NULL, &gpu->render_fence) != VK_SUCCESS)
        return error(err, cap, "Cannot create presentation rendering fence");
    return true;
}

