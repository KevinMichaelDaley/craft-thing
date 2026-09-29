#ifndef DUNGEONCRAFT_GPU_H
#define DUNGEONCRAFT_GPU_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

#ifndef DC_GPU_CHUNK_SLOTS
#define DC_GPU_CHUNK_SLOTS 64u
#endif

typedef struct {
    int32_t x_fp, y_fp;
    int32_t vx_fp, vy_fp;
    uint32_t width, height;
    uint32_t id, active;
} dc_gpu_body_t;

typedef struct dc_gpu dc_gpu_t;

typedef enum {
    DC_GPU_OVERLAY_NONE,
    DC_GPU_OVERLAY_RESIDENCY,
    DC_GPU_OVERLAY_STAGES
} dc_gpu_overlay_t;

typedef enum {
    DC_GPU_STAGE_RIGID = 1,
    DC_GPU_STAGE_FLUID = 2,
    DC_GPU_STAGE_SAND = 3
} dc_gpu_stage_id_t;

typedef struct {
    dc_gpu_stage_id_t id;
    uint64_t gpu_ns;
    uint32_t handoff;
} dc_gpu_stage_capture_t;

typedef struct {
    dc_gpu_stage_capture_t stages[3];
} dc_gpu_tick_capture_t;

typedef struct {
    uint64_t mapped_local_bytes;
    uint64_t mapped_system_bytes;
    uint64_t device_only_bytes;
} dc_gpu_memory_stats_t;

typedef struct {
    dc_cell_t cell;
    uint32_t resident;
    uint32_t slot;
} dc_gpu_halo_cell_t;

typedef enum {
    DC_GPU_TRANSFER_SCALAR = 1,
    DC_GPU_TRANSFER_PARTICLE = 2
} dc_gpu_transfer_kind_t;

typedef enum {
    DC_GPU_TRANSFER_PENDING = 0,
    DC_GPU_TRANSFER_APPLIED = 1,
    DC_GPU_TRANSFER_BLOCKED = 2
} dc_gpu_transfer_state_t;

typedef struct {
    uint32_t from_x, from_y, to_x, to_y;
    uint32_t amount;
    uint32_t kind;
    uint32_t state;
    uint32_t from_slot, to_slot, direct_slots;
} dc_gpu_transfer_t;

/** Create a headless Vulkan compute context and a width-by-height cell buffer. */
bool dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                   const char *shader_path, char *err_buf, uint32_t err_cap);
/** Create a headless context on an existing Vulkan device for GPU-only transfers. */
bool dc_gpu_create_shared(dc_gpu_t **out, dc_gpu_t *parent,
                          uint32_t width, uint32_t height,
                          const char *shader_path, char *err_buf,
                          uint32_t err_cap);

typedef struct {
    uint32_t main_slot, other_slot;
    uint32_t main_x, main_y, other_x, other_y;
    uint32_t other_side;
} dc_gpu_boundary_t;

/** Exchange conservative mass and face momentum across two GPU workspaces. */
bool dc_gpu_boundary_exchange(dc_gpu_t *main_gpu, dc_gpu_t *other_gpu,
                              const dc_gpu_boundary_t *boundaries,
                              uint32_t count, float elapsed_ticks,
                              char *err_buf, uint32_t err_cap);

/** Create the same compute context with an SDL Vulkan window for presentation. */
bool dc_gpu_create_window(dc_gpu_t **out, uint32_t width, uint32_t height,
                          uint32_t window_width, uint32_t window_height,
                          const char *shader_path, char *err_buf, uint32_t err_cap);
/** Select a camera crop inside the simulated grid for rendering and readback. */
bool dc_gpu_set_viewport(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                         uint32_t width, uint32_t height);
/** Set integer nearest-neighbor display scale within the Vulkan window. */
bool dc_gpu_set_display_zoom(dc_gpu_t *gpu, uint32_t zoom);
/** Convert a window pixel to a visible simulation cell; false in the letterbox. */
bool dc_gpu_screen_cell(dc_gpu_t *gpu, uint32_t screen_x, uint32_t screen_y,
                        uint32_t *cell_x, uint32_t *cell_y);
/** Choose a GPU-rendered debugging overlay. */
bool dc_gpu_set_overlay(dc_gpu_t *gpu, dc_gpu_overlay_t overlay);
/** Set the interactive Vulkan window caption. */
bool dc_gpu_set_window_title(dc_gpu_t *gpu, const char *title);
/** Rebase face velocities by whole chunks on the GPU after camera movement. */
bool dc_gpu_shift_velocity(dc_gpu_t *gpu, int32_t chunk_dx, int32_t chunk_dy,
                           char *err_buf, uint32_t err_cap);

/** Fill cells with a GPU-generated diagnostic pattern. */
bool dc_gpu_pattern(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Paint a solid disc of one RGBA8 color using a compute dispatch. */
bool dc_gpu_paint(dc_gpu_t *gpu, uint32_t x, uint32_t y, uint32_t radius,
                  uint32_t rgba, char *err_buf, uint32_t err_cap);

/** Upload one 64 x 64 world chunk into a bounded GPU slot. */
bool dc_gpu_upload_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                         char *err_buf, uint32_t err_cap);

/** Download one completed GPU chunk for persistence. */
bool dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                           char *err_buf, uint32_t err_cap);

/** Map a viewport chunk tile to a GPU slot; UINT32_MAX means unloaded. */
bool dc_gpu_set_page(dc_gpu_t *gpu, uint32_t tile_x, uint32_t tile_y,
                     uint32_t slot, char *err_buf, uint32_t err_cap);

/** Refresh one-cell halos from the current GPU page table. */
bool dc_gpu_refresh_halos(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Read a refreshed halo cell at local coordinates -1 through 64. */
bool dc_gpu_read_halo(dc_gpu_t *gpu, uint32_t tile_x, uint32_t tile_y,
                      int32_t local_x, int32_t local_y, dc_gpu_halo_cell_t *cell,
                      char *err_buf, uint32_t err_cap);

/** Queue one adjacent scalar or particle transfer between resident cells. */
bool dc_gpu_queue_transfer(dc_gpu_t *gpu, dc_gpu_transfer_t transfer,
                           char *err_buf, uint32_t err_cap);
/** Queue a transfer addressed by pinned chunk slots and local cells. */
bool dc_gpu_queue_slot_transfer(dc_gpu_t *gpu, dc_gpu_transfer_t transfer,
                                char *err_buf, uint32_t err_cap);

/** Refresh halos and retry the queued transfer; absent destinations stay pending. */
bool dc_gpu_try_transfer(dc_gpu_t *gpu, dc_gpu_transfer_state_t *state,
                         char *err_buf, uint32_t err_cap);

/** Render resident chunks through the GPU page table. */
bool dc_gpu_render_chunks(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Paint a material into the resident chunk atlas through a compute dispatch. */
bool dc_gpu_paint_material(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t radius, uint16_t material,
                           char *err_buf, uint32_t err_cap);

/** Set an optional GPU water source applied inside each physics submission. */
bool dc_gpu_set_tick_water_source(dc_gpu_t *gpu, bool enabled,
                                  uint32_t x, uint32_t y);

/** Spawn one box body in viewport-local 16.16 fixed-point coordinates. */
bool dc_gpu_spawn_body(dc_gpu_t *gpu, dc_gpu_body_t body,
                       char *err_buf, uint32_t err_cap);

/** Run GPU body integration, terrain contact, and occupancy raster passes. */
bool dc_gpu_rigid_step(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Advance one conservative Eulerian fluid substep on resident cells. */
bool dc_gpu_fluid_step(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Read the largest absolute discrete divergence in fully submerged cells. */
bool dc_gpu_fluid_max_divergence(dc_gpu_t *gpu, float *divergence,
                                 char *err_buf, uint32_t err_cap);

/** Enable or disable marker-guided volume correction; markers still move. */
bool dc_gpu_set_marker_correction(dc_gpu_t *gpu, bool enabled);

/** Toggle a GPU-rendered marker interface overlay. */
bool dc_gpu_set_marker_overlay(dc_gpu_t *gpu, bool enabled);

/** Read a chunk's sparse marker count for tests and streaming diagnostics. */
bool dc_gpu_marker_count(dc_gpu_t *gpu, uint32_t slot, uint32_t *count);

/** Submit rigid then fluid/sand probes and capture GPU timings and handoffs.
 * Fluid and sand probes verify ordering until their simulation shaders land. */
bool dc_gpu_tick_capture(dc_gpu_t *gpu, dc_gpu_tick_capture_t *capture,
                         char *err_buf, uint32_t err_cap);
bool dc_gpu_memory_stats(const dc_gpu_t *gpu, dc_gpu_memory_stats_t *stats);
/** Sample one four-side wet-edge mask per GPU chunk slot for sparse streaming. */
bool dc_gpu_wet_edge_masks(dc_gpu_t *gpu, uint32_t *masks, uint32_t mask_capacity,
                           char *err_buf, uint32_t err_cap);
/** Copy granular diagnostic scratch to mapped staging on explicit request. */
bool dc_gpu_mpm_readback_scratch(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Run the same GPU physics stages without reading diagnostics back. */
bool dc_gpu_tick_step(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Spread one Eulerian update across six ticks, or run it every tick (1). */
bool dc_gpu_set_fluid_interval(dc_gpu_t *gpu, uint32_t interval);
/** Set 4–32 red-black pressure sweeps between completed fluid solves. */
bool dc_gpu_set_pressure_sweeps(dc_gpu_t *gpu, uint32_t sweeps);
bool dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds);

/** Read the completed body state for tests or persistence. */
bool dc_gpu_read_body(dc_gpu_t *gpu, dc_gpu_body_t *body,
                      char *err_buf, uint32_t err_cap);

/** Present the current cell buffer to the Vulkan window. */
bool dc_gpu_present(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Render resident chunks and present them in one GPU submission. */
bool dc_gpu_present_chunks(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Execute pending physics ticks, render chunks, and present in one submission. */
bool dc_gpu_present_chunks_steps(dc_gpu_t *gpu, uint32_t steps,
                                 char *err_buf, uint32_t err_cap);

/** Copy completed GPU cell values into caller-owned memory. */
bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err_buf, uint32_t err_cap);

/** Destroy the context and its Vulkan resources. */
void dc_gpu_destroy(dc_gpu_t *gpu);

#endif
