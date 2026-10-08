#ifndef DUNGEONCRAFT_GPU_INTERNAL_H
#define DUNGEONCRAFT_GPU_INTERNAL_H

#include <SDL.h>
#include <vulkan/vulkan.h>

#include "dungeoncraft/gpu.h"

enum {
    DC_GPU_HALO_SIDE = DC_CHUNK_SIDE + 2,
    DC_GPU_HALO_CELLS = DC_GPU_HALO_SIDE * DC_GPU_HALO_SIDE
};

typedef struct {
    dc_gpu_body_t body;
    int32_t previous_x_fp, previous_y_fp;
    uint32_t reserved[2];
    uint32_t chunk_x[2], chunk_y[2];
    int32_t local_x_fp, local_y_fp;
    uint32_t visible, reserved_world;
    dc_gpu_body_shape_t shape;
    float angle, angular_velocity;
    uint32_t motion_flags, motion_ready;
    float center[2], inverse_mass, inverse_inertia;
    float mass, inertia, density, previous_angle;
} dc_gpu_body_record_t;

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
    VkBuffer chunk_staging_buffer;
    VkDeviceMemory chunk_memory;
    VkDeviceMemory chunk_staging_memory;
    void *chunk_mapped;
    VkBuffer particle_buffer, particle_count_buffer;
    VkBuffer particle_staging_buffer;
    VkDeviceMemory particle_memory, particle_count_memory;
    VkDeviceMemory particle_staging_memory;
    void *particle_mapped, *particle_count_mapped;
    VkBuffer mpm_proposal_buffer, mpm_output_buffer, mpm_grid_buffer;
    VkBuffer mpm_force_buffer, mpm_velocity_buffer, mpm_accept_buffer;
    VkBuffer mpm_force_staging_buffer, mpm_velocity_staging_buffer;
    VkDeviceMemory mpm_proposal_memory, mpm_output_memory, mpm_grid_memory;
    VkDeviceMemory mpm_force_memory, mpm_velocity_memory, mpm_accept_memory;
    VkDeviceMemory mpm_force_staging_memory, mpm_velocity_staging_memory;
    void *mpm_grid_mapped;
    void *mpm_force_mapped, *mpm_velocity_mapped;
    VkPipeline mpm_pipeline;
    VkBuffer mpm_activity_buffer;
    VkDeviceMemory mpm_activity_memory;
    VkPipeline mpm_activity_pipeline;
    VkBuffer mpm_label_a_buffer, mpm_label_b_buffer, mpm_component_size_buffer;
    VkDeviceMemory mpm_label_a_memory, mpm_label_b_memory, mpm_component_size_memory;
    void *mpm_label_a_mapped, *mpm_label_b_mapped, *mpm_component_size_mapped;
    VkPipeline mpm_component_pipeline;
    VkBuffer page_buffer;
    VkDeviceMemory page_memory;
    void *page_mapped;
    uint32_t page_width, page_height;
    uint32_t slot_page[DC_GPU_CHUNK_SLOTS];
    VkBuffer body_buffer;
    VkDeviceMemory body_memory;
    void *body_mapped;
    uint32_t body_ids[DC_GPU_BODY_CAPACITY];
    uint32_t body_count;
    bool rigid_occupancy_present;
    dc_chunk_coord_t body_origin;
    bool body_refresh_pending;
    VkBuffer occupancy_buffer;
    VkDeviceMemory occupancy_memory;
    void *occupancy_mapped;
    VkPipeline rigid_pipeline;
    VkPipeline broadphase_pipeline;
    VkPipeline contact_pipeline;
    VkPipeline solver_pipeline;
    bool rigid_solver_enabled;
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
    VkBuffer fluid_previous_buffer;
    VkDeviceMemory fluid_previous_memory;
    VkDeviceMemory fluid_a_memory;
    VkBuffer fluid_b_buffer;
    VkDeviceMemory fluid_b_memory;
    VkPipeline fluid_pipeline;
    VkBuffer velocity_buffer;
    VkDeviceMemory velocity_memory;
    void *velocity_mapped;
    dc_face_velocity_t *chunk_velocity;
    bool preserve_shifted_slot[DC_GPU_CHUNK_SLOTS];
    VkBuffer pressure_a_buffer;
    VkDeviceMemory pressure_a_memory;
    VkPipeline projection_pipeline;
    VkPipeline velocity_shift_pipeline;
    uint32_t fluid_tick;
    uint32_t fluid_interval;
    uint32_t fluid_phase;
    float tick_time_scale;
    float fluid_step_scale;
    float fluid_phase_budget;
    bool timed_fluid;
    bool fluid_snapshot_valid;
    VkDeviceSize mapped_local_bytes;
    VkDeviceSize mapped_system_bytes;
    VkDeviceSize device_only_bytes;
    VkBuffer marker_a_buffer, marker_b_buffer;
    VkDeviceMemory marker_a_memory, marker_b_memory;
    void *marker_a_mapped, *marker_b_mapped;
    VkBuffer marker_count_a_buffer, marker_count_b_buffer;
    VkDeviceMemory marker_count_a_memory, marker_count_b_memory;
    void *marker_count_a_mapped, *marker_count_b_mapped;
    VkBuffer marker_grid_buffer, slot_page_buffer, slot_seed_buffer;
    VkDeviceMemory marker_grid_memory, slot_page_memory, slot_seed_memory;
    void *marker_grid_mapped, *slot_page_mapped, *slot_seed_mapped;
    VkPipeline marker_pipeline;
    uint32_t marker_ping;
    bool marker_correction, marker_overlay;
    bool tick_water_source;
    uint32_t tick_water_x, tick_water_y;
    uint32_t width, height;
    uint32_t view_x, view_y, view_width, view_height;
    uint32_t display_zoom;
    dc_gpu_overlay_t overlay;
};

enum {
    DC_GPU_BROADPHASE_HEADER_WORDS = 8,
    DC_GPU_BROADPHASE_BUCKET_WORDS = 2,
    DC_GPU_BROADPHASE_GROUP_SIZE = 64,
    DC_GPU_BROADPHASE_CAPACITY_WORD = 3,
    DC_GPU_BROADPHASE_BUCKETS_WORD = 5
};

enum {
    DC_GPU_CONTACT_HEADER_WORDS = 12,
    DC_GPU_CONTACT_CAPACITY_WORD = 3,
    DC_GPU_CONTACT_DISPATCH_WORD = 6
};

static inline uint32_t dc_gpu_broadphase_buckets(const dc_gpu_t *gpu) {
    return (gpu->page_width + 2u) * (gpu->page_height + 2u);
}

static inline uint32_t *dc_gpu_broadphase_data(const dc_gpu_t *gpu) {
    return (uint32_t *)((dc_gpu_body_record_t *)gpu->body_mapped + DC_GPU_BODY_CAPACITY);
}

static inline uint32_t dc_gpu_contact_word_offset(const dc_gpu_t *gpu) {
    return DC_GPU_BROADPHASE_HEADER_WORDS +
        DC_GPU_BROADPHASE_BUCKET_WORDS * dc_gpu_broadphase_buckets(gpu) +
        DC_GPU_BROADPHASE_PAIR_CAPACITY * (sizeof(dc_gpu_broadphase_pair_t) / sizeof(uint32_t));
}

static inline uint32_t *dc_gpu_contact_data(const dc_gpu_t *gpu) {
    return dc_gpu_broadphase_data(gpu) + dc_gpu_contact_word_offset(gpu);
}

static inline VkDeviceSize dc_gpu_body_storage_bytes(const dc_gpu_t *gpu) {
    return sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
        (VkDeviceSize)(dc_gpu_contact_word_offset(gpu) + DC_GPU_CONTACT_HEADER_WORDS) * sizeof(uint32_t) +
        (VkDeviceSize)DC_GPU_CONTACT_CAPACITY * sizeof(dc_gpu_contact_t) +
        (16u + DC_GPU_BODY_CAPACITY * 32u + DC_GPU_CONTACT_CAPACITY * 16u) * sizeof(uint32_t);
}

static inline uint32_t dc_gpu_solver_word_offset(const dc_gpu_t *gpu) {
    return dc_gpu_contact_word_offset(gpu) + DC_GPU_CONTACT_HEADER_WORDS +
        DC_GPU_CONTACT_CAPACITY * (sizeof(dc_gpu_contact_t) / sizeof(uint32_t));
}
static inline uint32_t *dc_gpu_solver_data(const dc_gpu_t *gpu) {
    return dc_gpu_broadphase_data(gpu) + dc_gpu_solver_word_offset(gpu);
}

uint32_t dc_gpu_host_memory_type(const VkPhysicalDeviceMemoryProperties *props,
                                 uint32_t compatible_types);

static inline uint32_t dc_gpu_float_bits(float value) {
    union { float f; uint32_t u; } bits = { .f = value };
    return bits.u;
}

static inline uint32_t dc_gpu_render_flags(const dc_gpu_t *gpu) {
    uint32_t blend = gpu->fluid_snapshot_valid && gpu->fluid_interval == 6u ?
                     ((gpu->fluid_phase + 2u) % 6u) * 65535u / 6u : 65535u;
    return (blend << 8) | (gpu->marker_overlay ? 1u : 0u);
}

bool dc_gpu_pick_device(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap);
bool dc_gpu_load_shader_module(dc_gpu_t *gpu, const char *path,
                               VkShaderModule *module, char *err, uint32_t cap);
bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_chunks_destroy(dc_gpu_t *gpu);
bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap);
bool dc_gpu_make_device_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, char *err, uint32_t cap);
bool dc_gpu_copy_chunk_state(dc_gpu_t *gpu, uint32_t slot, bool upload,
                             bool particles, char *err, uint32_t cap);
bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_valid_body_shape(const dc_gpu_body_t *body, const dc_gpu_body_shape_t *shape);
bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *shader_path,
                                char *err, uint32_t cap);
void dc_gpu_rigid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_rigid(dc_gpu_t *gpu);
bool dc_gpu_broadphase_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_broadphase(dc_gpu_t *gpu);
bool dc_gpu_contact_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_contacts(dc_gpu_t *gpu);
bool dc_gpu_solver_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_solver(dc_gpu_t *gpu);
bool dc_gpu_tick_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_tick_destroy(dc_gpu_t *gpu);
bool dc_gpu_halo_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_halo_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_halo_destroy(dc_gpu_t *gpu);
bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_fluid_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_fluid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_fluid(dc_gpu_t *gpu);
void dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase);
void dc_gpu_record_tick_water_source(dc_gpu_t *gpu);
void dc_gpu_record_tick_step(dc_gpu_t *gpu);
bool dc_gpu_marker_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_marker_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_marker_destroy(dc_gpu_t *gpu);
void dc_gpu_record_markers(dc_gpu_t *gpu);
bool dc_gpu_mpm_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_mpm_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_mpm_buffers_destroy(dc_gpu_t *gpu);
void dc_gpu_mpm_pipeline_destroy(dc_gpu_t *gpu);
void dc_gpu_record_mpm(dc_gpu_t *gpu);

#endif
