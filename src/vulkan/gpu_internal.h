#ifndef DUNGEONCRAFT_GPU_INTERNAL_H
#define DUNGEONCRAFT_GPU_INTERNAL_H

#include <SDL.h>
#include <vulkan/vulkan.h>

#include "dungeoncraft/gpu.h"

struct dc_gpu {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    VkBuffer cells;
    VkDeviceMemory memory;
    void *mapped;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command;
    SDL_Window *window;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkImage *swap_images;
    uint32_t swap_count;
    VkExtent2D swap_extent;
    VkImage frame_image;
    VkDeviceMemory frame_memory;
    VkSemaphore acquire_sem;
    VkSemaphore present_sem;
    VkBuffer chunk_buffer;
    VkDeviceMemory chunk_memory;
    void *chunk_mapped;
    VkBuffer page_buffer;
    VkDeviceMemory page_memory;
    void *page_mapped;
    uint32_t page_width, page_height;
    VkBuffer body_buffer;
    VkDeviceMemory body_memory;
    void *body_mapped;
    VkBuffer occupancy_buffer;
    VkDeviceMemory occupancy_memory;
    void *occupancy_mapped;
    VkPipeline rigid_pipeline;
    uint32_t width, height;
};

bool dc_gpu_pick_device(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap);
bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_chunks_destroy(dc_gpu_t *gpu);
bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap);
bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *shader_path,
                                char *err, uint32_t cap);
void dc_gpu_rigid_destroy(dc_gpu_t *gpu);

#endif
