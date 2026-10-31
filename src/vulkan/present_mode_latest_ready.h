#ifndef DC_VULKAN_PRESENT_MODE_LATEST_READY_H
#define DC_VULKAN_PRESENT_MODE_LATEST_READY_H

#include <vulkan/vulkan.h>

/* Compatibility with Vulkan headers predating VK_EXT_present_mode_fifo_latest_ready. */
#define DC_VK_PRESENT_MODE_FIFO_LATEST_READY ((VkPresentModeKHR)1000361000)
#define DC_VK_PRESENT_MODE_FIFO_LATEST_READY_FEATURES ((VkStructureType)1000361000)
#define DC_VK_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION "VK_EXT_present_mode_fifo_latest_ready"

typedef struct {
    VkStructureType sType;
    void *pNext;
    VkBool32 presentModeFifoLatestReady;
} dc_vk_present_mode_fifo_latest_ready_features_t;

#endif
