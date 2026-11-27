#ifndef DC_VULKAN_SOFTWARE_PRESENT_H
#define DC_VULKAN_SOFTWARE_PRESENT_H

#include "gpu_internal.h"

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapped;
} dc_present_buffer_t;

typedef struct dc_software_present {
    VkDevice device;
    VkQueue queue;
    VkSwapchainKHR chain;
    VkImage *images;
    VkSemaphore *ready;
    uint32_t count;
    VkSemaphore acquire;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence upload_fence;
    VkImage composed;
    VkDeviceMemory composed_memory;
    VkDeviceSize bytes;
    dc_present_buffer_t readback, upload;
} dc_software_present_t;

/** Try an alternate X11 presentation device before creating a native swapchain. */
bool dc_software_present_create(dc_gpu_t *gpu, const VkSwapchainCreateInfoKHR *native_info);
/** Release window-owned presentation resources on their respective Vulkan devices. */
void dc_software_present_destroy(dc_gpu_t *gpu);
/** Upload and present an already completed frame at the acquired image index. */
VkResult dc_software_present_frame(dc_gpu_t *gpu, uint32_t index, void *capture);
/** Allocate a render-device transfer image for window composition. */
bool dc_window_transfer_image(dc_gpu_t *gpu, VkExtent2D extent, VkFormat format,
                              VkImage *image, VkDeviceMemory *memory, char *err, uint32_t cap);
#endif
