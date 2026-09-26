#ifndef DUNGEONCRAFT_GPU_INTERNAL_H
#define DUNGEONCRAFT_GPU_INTERNAL_H

#include <SDL.h>
#include <vulkan/vulkan.h>

#include "dungeoncraft/gpu.h"

enum {
    DC_GPU_HALO_SIDE = DC_CHUNK_SIDE + 2,
    DC_GPU_HALO_CELLS = DC_GPU_HALO_SIDE * DC_GPU_HALO_SIDE
};

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
    uint32_t slot_page[DC_GPU_CHUNK_SLOTS];
    VkBuffer body_buffer;
    VkDeviceMemory body_memory;
    void *body_mapped;
    VkBuffer occupancy_buffer;
    VkDeviceMemory occupancy_memory;
    void *occupancy_mapped;
    VkPipeline rigid_pipeline;
    VkBuffer trace_buffer;
    VkDeviceMemory trace_memory;
    void *trace_mapped;
    VkPipeline probe_pipeline;
    VkQueryPool timestamp_pool;
    float timestamp_period;
    uint32_t timestamp_bits;
    VkBuffer halo_buffer;
    VkDeviceMemory halo_memory;
    void *halo_mapped;
    VkBuffer transfer_buffer;
    VkDeviceMemory transfer_memory;
    void *transfer_mapped;
    VkPipeline halo_pipeline;
    bool has_transfer;
    VkBuffer fluid_a_buffer;
    VkDeviceMemory fluid_a_memory;
    void *fluid_a_mapped;
    VkBuffer fluid_b_buffer;
    VkDeviceMemory fluid_b_memory;
    void *fluid_b_mapped;
    VkPipeline fluid_pipeline;
    VkBuffer velocity_buffer;
    VkDeviceMemory velocity_memory;
    void *velocity_mapped;
    VkBuffer pressure_a_buffer;
    VkDeviceMemory pressure_a_memory;
    void *pressure_a_mapped;
    VkPipeline projection_pipeline;
    uint32_t fluid_tick;
    bool fluid_reset_velocity;
    bool tick_water_source;
    uint32_t tick_water_x, tick_water_y;
    uint32_t width, height;
};

bool dc_gpu_pick_device(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap);
bool dc_gpu_load_shader_module(dc_gpu_t *gpu, const char *path,
                               VkShaderModule *module, char *err, uint32_t cap);
bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_chunks_destroy(dc_gpu_t *gpu);
bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap);
bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *shader_path,
                                char *err, uint32_t cap);
void dc_gpu_rigid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_rigid(dc_gpu_t *gpu);
bool dc_gpu_tick_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_tick_destroy(dc_gpu_t *gpu);
bool dc_gpu_halo_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_halo_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_halo_destroy(dc_gpu_t *gpu);
bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_fluid_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_fluid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_fluid(dc_gpu_t *gpu);
void dc_gpu_record_tick_water_source(dc_gpu_t *gpu);
void dc_gpu_record_tick_step(dc_gpu_t *gpu);

#endif
