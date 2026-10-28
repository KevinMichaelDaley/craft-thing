#ifndef DUNGEONCRAFT_GPU_INTERNAL_H
#define DUNGEONCRAFT_GPU_INTERNAL_H

#include <SDL.h>
#include <vulkan/vulkan.h>

#include "dungeoncraft/contact_work_layout.h"
#include "dungeoncraft/gpu.h"
#include "dungeoncraft/color.h"
#include "ferrum/structural/gpu.h"
#include "ferrum/fire/reactive_chunk.h"
#include "mpm_activity_layout.h"

enum {
    DC_GPU_DESCRIPTOR_BINDINGS = 37,
    DC_GPU_STRUCTURAL_TAG = 0x53545231u,
    DC_STRUCTURAL_HEADER_WORDS = 32,
    DC_GPU_PIXEL_BODY_FLAG = 0x80000000u,
    DC_GPU_PIXEL_POOL_HEADER_WORDS = 16,
    DC_GPU_INITIAL_PIXEL_POOL_WORDS = 8192,
    DC_STRUCTURAL_COMPONENT_DIRTY_WORD = 20,
    DC_STRUCTURAL_BRUSH_RESULT_WORD = 21,
    DC_STRUCTURAL_GRAPH_WORDS = 2320,
    DC_STRUCTURAL_RESERVE_WORDS = 40000,
    DC_GPU_STRUCTURAL_CONVERTED_TAG = 0x53545232u,
    DC_STRUCTURAL_NODE_STATE_WORDS = 12,
    DC_GPU_HALO_SIDE = DC_CHUNK_SIDE + 2,
    DC_GPU_HALO_CELLS = DC_GPU_HALO_SIDE * DC_GPU_HALO_SIDE,
    DC_GPU_SOLVER_HEADER_WORDS = 16,
    DC_GPU_PIXEL_CONTACT_DISPATCH_WORD = 10,
    DC_GPU_TYPED_CONTACT_ACTIVITY_WORD = DC_CONTACT_TYPED_ACTIVITY_WORD,
    DC_GPU_CONTACT_PHASE_COUNT = 3,
    DC_GPU_SOLVER_BODY_WORDS = 32,
    DC_GPU_SOLVER_CONTACT_WORDS = 16,
    DC_GPU_SOLVER_PHASE_COUNT = 25,
    DC_GPU_MPM_PHASE_COUNT = 8,
    DC_GPU_STRUCTURAL_PHASE_COUNT = 7
};

typedef struct {
    dc_gpu_body_t body;
    int32_t previous_x_fp, previous_y_fp;
    uint32_t reserved[2];
    uint32_t chunk_x[2], chunk_y[2];
    int32_t local_x_fp, local_y_fp;
    uint32_t visible;
    float radius;
    dc_gpu_body_shape_t shape;
    float angle, angular_velocity;
    uint32_t motion_flags, motion_ready;
    float center[2], inverse_mass, inverse_inertia;
    float mass, inertia, density, previous_angle;
    int32_t bounds_low[2], bounds_high[2], swept_low[2], swept_high[2];
    uint32_t piece_count, compound_reserved;
    dc_gpu_body_shape_t pieces[DC_GPU_BODY_PIECES];
    int32_t rest_position[2];
    float rest_angle;
    uint32_t rest_ticks;
} dc_gpu_body_record_t;

struct dc_gpu {
    struct fr_reactive_gpu *reactive;
    struct fr_revolute_gpu *revolute;
    struct fr_thermal_gpu *thermal;
    struct fr_gas_gpu *gas;
    struct dc_gpu_authored_stage *authored_stage;
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
    VkDescriptorSet mpm_swapped_descriptor;
    VkPipeline mpm_swap_commit_pipeline;
    uint64_t mpm_swap_descriptor_revision;
    bool mpm_swap_buffers;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkPipeline simple_draw_pipeline, pixel_draw_pipeline;
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
    VkSemaphore *present_sems;
    VkFence render_fence;
    VkFence tick_fence;
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
    VkPipeline mpm_phase_pipelines[DC_GPU_MPM_PHASE_COUNT];
    bool mpm_fused_grid;
    VkBuffer mpm_activity_buffer;
    VkDeviceMemory mpm_activity_memory;
    VkPipeline mpm_activity_pipeline;
    VkPipeline mpm_compaction_pipelines[DC_MPM_COMPACTION_PHASES];
    dc_gpu_mpm_scan_layout_t mpm_scan_layout;
    VkBuffer mpm_label_a_buffer, mpm_label_b_buffer, mpm_component_size_buffer;
    VkDeviceMemory mpm_label_a_memory, mpm_label_b_memory, mpm_component_size_memory;
    void *mpm_label_a_mapped, *mpm_label_b_mapped, *mpm_component_size_mapped;
    VkPipeline mpm_component_pipeline, component_extract_pipelines[38];
    VkPipeline component_object_pipelines[39];
    uint64_t authored_objects_address,authored_objects_transaction;
    bool automatic_components_enabled;
    bool cold_component_admission;
    bool component_baseline_pending;
    VkBuffer page_buffer;
    VkDeviceMemory page_memory;
    void *page_mapped;
    uint32_t page_width, page_height;
    uint32_t slot_page[DC_GPU_CHUNK_SLOTS];
    VkBuffer body_buffer;
    VkDeviceMemory body_memory;
    void *body_mapped;
    uint32_t *body_ids;
    struct dc_runtime_bodies *runtime_bodies;
    bool runtime_body_consumers;
    uint32_t runtime_body_slots;
    uint32_t pixel_pool_words;
    uint32_t runtime_pair_capacity,runtime_contact_capacity;
    VkDeviceAddress shape_color_address;
    VkDeviceSize shape_color_allocation_bytes;
    bool shape_color_device_local;
    uint32_t body_count;
    bool rigid_occupancy_present;
    dc_chunk_coord_t body_origin;
    bool body_refresh_pending;
    VkBuffer occupancy_buffer;
    VkDeviceMemory occupancy_memory;
    void *occupancy_mapped;
    VkPipeline rigid_pipeline;
    VkPipeline rigid_pixel_pipeline,rigid_classify_pipeline;
    VkPipeline rigid_prepare_pipeline;
    VkPipeline broadphase_pipeline;
    VkPipeline contact_pipelines[DC_GPU_CONTACT_PHASE_COUNT];
    VkPipeline contact_pixel_pipeline, contact_pixel_plan_pipeline,
               contact_pixel_finish_pipeline, contact_pixel_work_pipeline;
    VkPipeline contact_terrain_pipeline;
    VkPipeline contact_boundary_pipeline;
    bool contact_boundary_cache_enabled;
    bool contact_caches_reusable;
    VkPipeline solver_pipelines[DC_GPU_SOLVER_PHASE_COUNT];
    VkPipeline solver_unstructured[2];
    VkPipeline pixel_hydro_pipelines[5];
    VkPipeline structural_pipelines[DC_GPU_STRUCTURAL_PHASE_COUNT];
    bool structural_enabled;
    bool structural_graph_possible;
    dc_chunk_t *checkpoint_chunks;
    fr_reactive_chunk_t *checkpoint_reactive;
    uint32_t **checkpoint_reactive_handles;
    bool reactive_checkpoint_installing;
    dc_chunk_colors_t *checkpoint_colors;
    uint32_t checkpoint_chunk_count;
    bool impact_fracture_enabled;
    bool rigid_solver_enabled;
    bool solver_fused_grain;
    bool solver_grouped_reductions;
    bool solver_skip_zero_reductions;
    bool solver_apply_overlap;
    VkBuffer trace_buffer;
    VkDeviceMemory trace_memory;
    void *trace_mapped;
    VkPipeline probe_pipeline;
    VkQueryPool timestamp_pool;
    bool component_profile;
    bool solver_recording;
    bool solver_cache_disabled;
    uint64_t descriptor_revision;
    struct dc_solver_commands *solver_commands;
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
    VkBuffer water_color_buffer;
    VkDeviceMemory water_color_memory;
    void *water_color_mapped;
    VkBuffer thermal_control_buffer;
    VkDeviceMemory thermal_control_memory;
    void *thermal_control_mapped;
    VkBuffer shape_color_buffer;
    VkDeviceMemory shape_color_memory;
    void *shape_color_mapped;
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
    VkBuffer pressure_work_buffer;
    VkDeviceMemory pressure_work_memory;
    VkBuffer pressure_stencil_buffer;
    VkDeviceMemory pressure_stencil_memory;
    void *pressure_work_mapped;
    VkDescriptorSetLayout pressure_work_set_layout;
    VkDescriptorPool pressure_work_pool;
    VkDescriptorSet pressure_work_descriptor;
    VkPipelineLayout pressure_pipeline_layout;
    VkPipeline pressure_work_pipelines[3];
    bool pressure_work_enabled;
    VkPipeline hydro_head_pipeline;
    VkPipeline fluid_aperture_pipeline;
    VkPipeline fluid_transport_pipelines[4];
    VkPipeline fluid_displacement_pipelines[2];
    VkPipeline fluid_phase_pipelines[13];
    VkPipeline projection_phase_pipelines[7];
    VkPipeline velocity_shift_pipeline;
    uint32_t fluid_tick;
    uint32_t fluid_interval;
    uint32_t fluid_phase;
    float tick_time_scale;
    float fluid_step_scale;
    float fluid_phase_budget;
    bool timed_fluid;
    bool fluid_snapshot_valid;
    bool fluid_work_possible;
    uint8_t fluid_uploaded_state[DC_GPU_CHUNK_SLOTS]; /* 0 unknown, 1 dry, 2 active. */
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
    dc_presentation_focus_t presentation_focus;
    dc_gpu_overlay_t overlay;
    struct fr_scene_gpu *renderer;
};

bool fr_renderer_active(const dc_gpu_t *gpu);
void fr_renderer_record(dc_gpu_t *gpu);
void fr_renderer_destroy(dc_gpu_t *gpu);

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
        (gpu->runtime_pair_capacity?gpu->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY) * (sizeof(dc_gpu_broadphase_pair_t) / sizeof(uint32_t));
}

static inline uint32_t *dc_gpu_contact_data(const dc_gpu_t *gpu) {
    return dc_gpu_broadphase_data(gpu) + dc_gpu_contact_word_offset(gpu);
}

static inline VkDeviceSize dc_gpu_legacy_body_storage_bytes(const dc_gpu_t *gpu) {
    return sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
        (VkDeviceSize)(dc_gpu_contact_word_offset(gpu) + DC_GPU_CONTACT_HEADER_WORDS) * sizeof(uint32_t) +
        (VkDeviceSize)(gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY) * sizeof(dc_gpu_contact_t) +
        (DC_GPU_SOLVER_HEADER_WORDS + DC_GPU_BODY_CAPACITY * DC_GPU_SOLVER_BODY_WORDS +
         (gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY) * DC_GPU_SOLVER_CONTACT_WORDS) * sizeof(uint32_t) +
        (DC_STRUCTURAL_HEADER_WORDS + DC_GPU_CHUNK_SLOTS*8u + DC_STRUCTURAL_RESERVE_WORDS)*sizeof(uint32_t);
}

/** Terrain scratch covers resident cells independently of growing body geometry. */
static inline VkDeviceSize dc_gpu_component_storage_bytes(const dc_gpu_t *gpu) {
    VkDeviceSize visible=(VkDeviceSize)gpu->width*gpu->height;
    VkDeviceSize resident=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS;
    return (2u*(visible>resident?visible:resident)+DC_CHUNK_CELLS)*sizeof(uint32_t);
}
static inline uint32_t dc_gpu_pixel_pool_capacity(const dc_gpu_t *gpu) {
    if(gpu->pixel_pool_words) return gpu->pixel_pool_words;
    uint32_t cells = gpu->width * gpu->height;
    uint32_t resident = DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS;
    return 2u * (((cells > resident ? cells : resident) + 7u) / 8u) +
           8u * DC_GPU_BODY_CAPACITY;
}
static inline uint32_t dc_gpu_pixel_pool_word_offset(const dc_gpu_t *gpu) {
    return (uint32_t)(dc_gpu_legacy_body_storage_bytes(gpu) / sizeof(uint32_t));
}
/** Validate a complete tiled pixel shape during explicit diagnostics/checkpoint IO. */
bool dc_gpu_pixel_geometry_valid(const dc_gpu_body_record_t *body,
                                 const uint32_t *geometry, uint32_t words,
                                 float *density);
/** Upgrade version-four dense geometry at an explicit checkpoint restore boundary. */
bool dc_gpu_upgrade_dense_pixels(dc_gpu_t *gpu, dc_gpu_body_record_t *bodies,
                                 uint32_t **pixels, uint32_t *words);
static inline uint32_t dc_gpu_contact_work_word_offset(const dc_gpu_t *gpu) {
    return dc_gpu_pixel_pool_word_offset(gpu)+DC_GPU_PIXEL_POOL_HEADER_WORDS+
        2u*dc_gpu_pixel_pool_capacity(gpu);
}
static inline VkDeviceSize dc_gpu_contact_work_bytes(const dc_gpu_t *gpu) {
    uint32_t pairs=gpu->runtime_pair_capacity?gpu->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY;
    VkDeviceSize chunks=(VkDeviceSize)((gpu->width+63u)/64u)*((gpu->height+63u)/64u);
    VkDeviceSize boundaries=gpu->contact_boundary_cache_enabled?
        2u*(VkDeviceSize)dc_gpu_pixel_pool_capacity(gpu):0u;
    return gpu->runtime_bodies?(4u+(VkDeviceSize)pairs*DC_CONTACT_PIXEL_GROUPS*2u+chunks+boundaries)*sizeof(uint32_t):0u;
}
static inline VkDeviceSize dc_gpu_body_storage_bytes(const dc_gpu_t *gpu) {
    return dc_gpu_legacy_body_storage_bytes(gpu) +
        (DC_GPU_PIXEL_POOL_HEADER_WORDS + 2u * dc_gpu_pixel_pool_capacity(gpu)) * sizeof(uint32_t)+
        dc_gpu_contact_work_bytes(gpu);
}

static inline uint32_t dc_gpu_solver_word_offset(const dc_gpu_t *gpu) {
    return dc_gpu_contact_word_offset(gpu) + DC_GPU_CONTACT_HEADER_WORDS +
        (gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY) * (sizeof(dc_gpu_contact_t) / sizeof(uint32_t));
}
static inline uint32_t *dc_gpu_solver_data(const dc_gpu_t *gpu) {
    return dc_gpu_broadphase_data(gpu) + dc_gpu_solver_word_offset(gpu);
}

static inline uint32_t dc_gpu_structural_word_offset(const dc_gpu_t *gpu) {
    return dc_gpu_solver_word_offset(gpu) + DC_GPU_SOLVER_HEADER_WORDS +
        DC_GPU_BODY_CAPACITY*DC_GPU_SOLVER_BODY_WORDS +
        (gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY)*DC_GPU_SOLVER_CONTACT_WORDS;
}
static inline uint32_t *dc_gpu_structural_data(const dc_gpu_t *gpu) {
    return dc_gpu_broadphase_data(gpu)+dc_gpu_structural_word_offset(gpu);
}
static inline uint32_t dc_gpu_structural_graph_offset(void) {
    return DC_STRUCTURAL_HEADER_WORDS+DC_GPU_CHUNK_SLOTS*8u;
}
bool dc_gpu_structural_pipeline_init(dc_gpu_t *gpu,char *err,uint32_t cap);
void dc_gpu_authored_destroy(dc_gpu_t *gpu);
void dc_gpu_record_structural(dc_gpu_t *gpu);
void dc_gpu_record_structural_begin(dc_gpu_t *gpu);
/** Record topology reconstruction at an explicit restore boundary. */
void dc_gpu_record_structural_topology(dc_gpu_t *gpu);
void dc_gpu_record_body_carve(dc_gpu_t *gpu,uint32_t x,uint32_t y,uint32_t radius);
/** Reserve loose ash before publishing cooperative burnt-rest fragments. */
void dc_gpu_record_body_ash(dc_gpu_t *gpu,VkBuffer indirect);
void dc_gpu_structural_track_chunk(dc_gpu_t *gpu,uint32_t slot,dc_chunk_coord_t coord);
bool dc_gpu_upload_chunk_exact(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                              bool seed, char *err, uint32_t cap);
bool dc_gpu_upload_chunk_checkpoint(dc_gpu_t *gpu, uint32_t slot,
                                    const dc_chunk_t *chunk, bool seed,
                                    bool *restored, char *err, uint32_t cap);

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
void dc_gpu_fluid_uploads_invalidate(dc_gpu_t *gpu);
void dc_gpu_fluid_uploads_recheck(dc_gpu_t *gpu);
bool dc_gpu_copy_chunk_state(dc_gpu_t *gpu, uint32_t slot, bool upload,
                             bool particles, char *err, uint32_t cap);
bool dc_gpu_rigid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_valid_body_shape(const dc_gpu_body_t *body, const dc_gpu_body_shape_t *shape);
bool dc_gpu_valid_compound_shape(const dc_gpu_body_t *body, const dc_gpu_compound_shape_t *shape);
bool dc_gpu_rigid_pipeline_init(dc_gpu_t *gpu, const char *shader_path,
                                char *err, uint32_t cap);
void dc_gpu_rigid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_rigid(dc_gpu_t *gpu);
bool dc_gpu_broadphase_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_broadphase(dc_gpu_t *gpu);
/** Reconcile GPU-born owners only at explicit authoring/diagnostic checkpoints. */
void dc_gpu_refresh_body_registry(dc_gpu_t *gpu);
/** Allocated runtime slots; allocation granularity is not a body-count ceiling. */
uint32_t dc_gpu_body_capacity(const dc_gpu_t *gpu);
/** Direct mapped access, restricted to explicit host authoring/diagnostics. */
dc_gpu_body_record_t *dc_gpu_body_record(const dc_gpu_t *gpu,uint32_t slot);
/** Fixed GPU bound lets new descendants participate without frame-time readback. */
static inline uint32_t dc_gpu_runtime_body_count(const dc_gpu_t *gpu) {
    return gpu->structural_enabled ? (gpu->runtime_body_slots?gpu->runtime_body_slots:DC_GPU_BODY_CAPACITY) : gpu->body_count;
}
bool dc_gpu_contact_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_contacts(dc_gpu_t *gpu);
/** Refresh GPU chunk collision presence before an immutable-terrain solver batch. */
void dc_gpu_record_contact_terrain(dc_gpu_t *gpu);
bool dc_gpu_solver_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Prepare graph-only solver variants before publishing an authored/restored graph. */
bool dc_gpu_solver_graph_prepare(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_solver(dc_gpu_t *gpu);
/** Record the reference solver sequence for cache rebuilds and diagnostics. */
void dc_gpu_record_solver_uncached(dc_gpu_t *gpu);
/** Allocate reusable solver commands during context initialization. */
bool dc_gpu_solver_commands_init(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Release cached solver commands before destroying their command pool. */
void dc_gpu_solver_commands_destroy(dc_gpu_t *gpu);
/** Read CPU recording counters without reading simulation payloads. */
bool dc_gpu_solver_commands_stats(const dc_gpu_t *gpu, uint64_t *recordings, uint64_t *executions);
bool dc_gpu_tick_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_tick_destroy(dc_gpu_t *gpu);
bool dc_gpu_halo_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_halo_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_halo_destroy(dc_gpu_t *gpu);
bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_fluid_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_pressure_work_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_pressure_work_destroy(dc_gpu_t *gpu);
void dc_gpu_record_pressure_work(dc_gpu_t *gpu);
void dc_gpu_fluid_destroy(dc_gpu_t *gpu);
void dc_gpu_record_fluid(dc_gpu_t *gpu);
void dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase);
void dc_gpu_record_fluid_phase_uncached(dc_gpu_t *gpu, uint32_t phase);
void dc_gpu_record_tick_water_source(dc_gpu_t *gpu);
/** Initialize dry fluid scratch before scheduling can omit its zero work. */
bool dc_gpu_clear_fluid_scratch(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_record_tick_step(dc_gpu_t *gpu);
bool dc_gpu_marker_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_marker_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
void dc_gpu_marker_destroy(dc_gpu_t *gpu);
void dc_gpu_record_markers(dc_gpu_t *gpu);
bool dc_gpu_mpm_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap);
bool dc_gpu_mpm_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Prepare the metadata-only commit variant without allocating another particle pool. */
bool dc_gpu_mpm_swap_init(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Mirror immutable descriptors and exchange only the two existing particle bindings. */
void dc_gpu_mpm_swap_descriptors(dc_gpu_t *gpu);
void dc_gpu_mpm_buffers_destroy(dc_gpu_t *gpu);
void dc_gpu_mpm_pipeline_destroy(dc_gpu_t *gpu);
void dc_gpu_record_mpm(dc_gpu_t *gpu);
/** Record the reference MPM sequence for cache rebuilds and diagnostics. */
void dc_gpu_record_mpm_uncached(dc_gpu_t *gpu);
bool dc_gpu_mpm_compaction_pipeline_init(dc_gpu_t *gpu,char *err,uint32_t cap);
void dc_gpu_mpm_compaction_pipeline_destroy(dc_gpu_t *gpu);
/** Record deterministic compaction with compatible descriptors already bound. */
void dc_gpu_record_active_compaction(dc_gpu_t *gpu,bool rigid);
void dc_gpu_record_mpm_activity(dc_gpu_t *gpu,bool rigid);
/** Create exact render variants sharing the ordinary descriptor interface. */
bool dc_gpu_draw_pipeline_init(dc_gpu_t *gpu,VkShaderModule module,char *err,uint32_t cap);
/** Record a draw with GPU body-type selection and the configured viewport. */
void dc_gpu_record_draw(dc_gpu_t *gpu,const uint32_t push[7]);
/** Initialize the window swapchain and its image-owned synchronization. */
bool dc_gpu_window_init(dc_gpu_t *gpu, uint32_t width, uint32_t height,
                        char *err, uint32_t cap);

void dc_gpu_record_components(dc_gpu_t *gpu, bool rigid);
void dc_gpu_record_component_extraction(dc_gpu_t *gpu);
bool dc_gpu_colors_init(dc_gpu_t *gpu,char *err,uint32_t cap);
void dc_gpu_colors_destroy(dc_gpu_t *gpu);
VkDeviceSize dc_gpu_color_terrain_bytes(void);
VkDeviceSize dc_gpu_color_bank_bytes(const dc_gpu_t *gpu);
float *dc_gpu_shape_colors(dc_gpu_t *gpu,uint32_t absolute_word);
void dc_gpu_read_chunk_colors(dc_gpu_t *gpu,uint32_t slot,dc_chunk_colors_t *colors);
void dc_gpu_write_chunk_colors(dc_gpu_t *gpu,uint32_t slot,const dc_chunk_colors_t *colors);
void dc_gpu_record_component_baseline(dc_gpu_t *gpu);

#endif
