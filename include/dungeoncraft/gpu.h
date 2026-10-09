#ifndef DUNGEONCRAFT_GPU_H
#define DUNGEONCRAFT_GPU_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

enum { DC_GPU_BODY_KINEMATIC = 1u, DC_GPU_BODY_LOCK_ROTATION = 2u };
/** Completed angular state and material-derived properties. Density is relative
 * to water (1); mass and inertia use cell area with unit out-of-plane thickness. */
typedef struct {
    float angle, angular_velocity, mass, inertia, density, center_x, center_y;
    uint32_t flags;
} dc_gpu_body_motion_t;
typedef struct {
    uint32_t enabled, iterations, substeps, overflow, solved_contacts, active_bodies;
} dc_gpu_rigid_solver_stats_t;

#ifndef DC_GPU_CHUNK_SLOTS
#define DC_GPU_CHUNK_SLOTS 64u
#endif

#define DC_GPU_BODY_CAPACITY 64u
#define DC_GPU_BODY_MAX_SIDE 16u

typedef struct {
    int32_t x_fp, y_fp;
    int32_t vx_fp, vy_fp;
    uint32_t width, height;
    uint32_t id, active;
} dc_gpu_body_t;

typedef struct dc_gpu dc_gpu_t;

/** Enable the GPU contact solver; disabled contexts retain legacy diagnostic motion. */
bool dc_gpu_set_rigid_solver(dc_gpu_t *gpu, bool enabled);
/** Set angular state and kinematic/rotation-lock flags. Density comes from material.
 * Linear components are bounded to four cells/tick. Eight substeps and a
 * radius-dependent angular limit keep predicted vertex motion below one cell. */
bool dc_gpu_set_body_motion(dc_gpu_t *gpu, uint32_t id, float angle, float angular_velocity,
                            uint32_t flags, char *err_buf, uint32_t err_cap);
/** Explicit diagnostic readback of GPU-owned motion and derived physical properties. */
bool dc_gpu_read_body_motion(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_motion_t *motion,
                             char *err_buf, uint32_t err_cap);
/** Explicit diagnostic readback; the ordinary solve never reads GPU counters. */
bool dc_gpu_read_rigid_solver_stats(dc_gpu_t *gpu, dc_gpu_rigid_solver_stats_t *stats);

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

/** Spawn or replace a stable-ID box in viewport-local 16.16 coordinates.
 * Up to DC_GPU_BODY_CAPACITY boxes may be active; new IDs fail when full.
 */
bool dc_gpu_spawn_body(dc_gpu_t *gpu, dc_gpu_body_t body,
                       char *err_buf, uint32_t err_cap);

/** Read a completed body by stable nonzero ID; unknown IDs leave output unchanged. */
bool dc_gpu_read_body_id(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_t *body,
                         char *err_buf, uint32_t err_cap);

/** Remove a body by ID; its occupancy clears on the next rigid step. */
bool dc_gpu_remove_body(dc_gpu_t *gpu, uint32_t id,
                        char *err_buf, uint32_t err_cap);

/** World position is a signed chunk anchor plus body.x_fp/y_fp within that chunk. */
typedef struct {
    dc_chunk_coord_t chunk;
    dc_gpu_body_t body;
} dc_gpu_world_body_t;

#define DC_GPU_CONVEX_VERTICES 8u
enum { DC_GPU_BODY_STONE = 1u, DC_GPU_BODY_WOOD = 2u };
/** Strictly convex counterclockwise body-local 16.16 vertices within box bounds.
 * Rotation is baked into the vertices. count=0 denotes the legacy box.
 */
typedef struct {
    uint32_t count, material;
    struct { int32_t x_fp, y_fp; } vertices[DC_GPU_CONVEX_VERTICES];
} dc_gpu_body_shape_t;
#define DC_GPU_BODY_PIECES 4u
/** Connected, non-overlapping convex pieces sharing one rigid pose and material. */
typedef struct {
    uint32_t count;
    dc_gpu_body_shape_t pieces[DC_GPU_BODY_PIECES];
} dc_gpu_compound_shape_t;
/** Spawn a compound body; invalid/disconnected/overlapping pieces leave state unchanged. */
bool dc_gpu_spawn_compound_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                                const dc_gpu_compound_shape_t *shape,
                                char *err_buf, uint32_t err_cap);
/** Read compound pieces by stable ID; legacy convex bodies have count zero. */
bool dc_gpu_read_compound_shape(dc_gpu_t *gpu, uint32_t id, dc_gpu_compound_shape_t *shape,
                                char *err_buf, uint32_t err_cap);
/** Spawn/update a world body with a validated convex shape, atomically on error. */
bool dc_gpu_spawn_convex_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                              const dc_gpu_body_shape_t *shape,
                              char *err_buf, uint32_t err_cap);
/** Read a completed body's shape by stable ID; unknown IDs leave output unchanged. */
bool dc_gpu_read_body_shape(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_shape_t *shape,
                             char *err_buf, uint32_t err_cap);

/** Set the world chunk represented by simulation cell (0,0), without advancing time. */
bool dc_gpu_set_body_origin(dc_gpu_t *gpu, dc_chunk_coord_t origin,
                             char *err_buf, uint32_t err_cap);
/** Spawn or update a world-anchored body, including bodies outside the viewport. */
bool dc_gpu_spawn_world_body(dc_gpu_t *gpu, dc_gpu_world_body_t body,
                              char *err_buf, uint32_t err_cap);
/** Read completed world position and velocity for diagnostics or persistence. */
bool dc_gpu_read_world_body(dc_gpu_t *gpu, uint32_t id, dc_gpu_world_body_t *body,
                             char *err_buf, uint32_t err_cap);
/** Atomically save the world body pool at a completed GPU boundary. */
bool dc_gpu_save_bodies(dc_gpu_t *gpu, const char *path, char *err_buf, uint32_t err_cap);
/** Load a validated snapshot; a missing file leaves the initial pool unchanged. */
bool dc_gpu_load_bodies(dc_gpu_t *gpu, const char *path, char *err_buf, uint32_t err_cap);
/** Return an unused nonzero body ID without reading GPU transforms. */
uint32_t dc_gpu_next_body_id(const dc_gpu_t *gpu);

#define DC_GPU_BROADPHASE_PAIR_CAPACITY 4096u
enum { DC_GPU_PAIR_BODY = 0u, DC_GPU_PAIR_TERRAIN = 1u, DC_GPU_PAIR_BOUNDARY = 2u };
/** Stable candidate key; terrain/boundary pairs have body_b=0 and a world chunk. */
typedef struct {
    uint32_t body_a, body_b, kind, reserved;
    dc_chunk_coord_t chunk;
} dc_gpu_broadphase_pair_t;
/** Overflow flags are nonzero if the candidate set is incomplete. */
typedef struct {
    uint32_t count, required, overflow, capacity, active_bodies, buckets;
} dc_gpu_broadphase_stats_t;
/** Set a bounded candidate limit for diagnostics; overflow must gate contact solving. */
bool dc_gpu_set_broadphase_capacity(dc_gpu_t *gpu, uint32_t capacity,
                                    char *err_buf, uint32_t err_cap);
/** Opt-in completed candidate readback; output ordering is unspecified. */
bool dc_gpu_read_broadphase(dc_gpu_t *gpu, dc_gpu_broadphase_stats_t *stats,
                            dc_gpu_broadphase_pair_t *pairs, uint32_t pair_cap,
                            char *err_buf, uint32_t err_cap);

#define DC_GPU_CONTACT_CAPACITY 8192u
#define DC_GPU_CONTACT_FEATURE_EDGE 0x80000000u
enum { DC_GPU_CONTACT_BODY = 0u, DC_GPU_CONTACT_TERRAIN = 1u,
       DC_GPU_CONTACT_BOUNDARY = 2u, DC_GPU_CONTACT_MPM = 3u };
enum { DC_GPU_CONTACT_OVERFLOW_CAPACITY = 1u, DC_GPU_CONTACT_OVERFLOW_WORLD = 2u,
       DC_GPU_CONTACT_OVERFLOW_BROADPHASE = 4u };
/** Canonical world point, with local 16.16 coordinates in [0,64). */
typedef struct {
    dc_chunk_coord_t chunk;
    int32_t x_fp, y_fp;
} dc_gpu_contact_anchor_t;
/** Normal points from B to A; depth is nonnegative. Terrain feature_b is a
 * cell index within feature_chunk; body features encode vertices/edges.
 * Body material IDs and cell material IDs use their respective namespaces.
 */
typedef struct {
    uint32_t body_a, body_b, kind, feature_a, feature_b, material_a, material_b, reserved;
    float normal_x, normal_y, depth, friction, restitution, compliance;
    uint32_t flags, reserved2;
    dc_gpu_contact_anchor_t anchor_a, anchor_b;
    dc_chunk_coord_t feature_chunk;
} dc_gpu_contact_t;
/** Any overflow makes contacts incomplete and must gate the later solver. */
typedef struct {
    uint32_t count, required, overflow, capacity, candidates, rejected;
} dc_gpu_contact_stats_t;
/** Bound contact output for diagnostics; 0 is allowed to test saturation. */
bool dc_gpu_set_contact_capacity(dc_gpu_t *gpu, uint32_t capacity,
                                 char *err_buf, uint32_t err_cap);
/** Opt-in completed GPU contacts; ordering is unspecified, keys are stable. */
bool dc_gpu_read_contacts(dc_gpu_t *gpu, dc_gpu_contact_stats_t *stats,
                          dc_gpu_contact_t *contacts, uint32_t contact_cap,
                          char *err_buf, uint32_t err_cap);

/** Read current and conservative swept-AABB IDs for one completed cell.
 * Overlapping masks select the lowest nonzero ID. Fluid uses current occupancy.
 */
bool dc_gpu_read_occupancy(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t *current, uint32_t *swept,
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
/** Copy granular diagnostic scratch to mapped staging on explicit request. */
bool dc_gpu_mpm_readback_scratch(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Run the same GPU physics stages without reading diagnostics back. */
bool dc_gpu_tick_step(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Spread one Eulerian update across six ticks, or run it every tick (1). */
bool dc_gpu_set_fluid_interval(dc_gpu_t *gpu, uint32_t interval);
bool dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds);

/** Read the first active completed body, or a zero body when the pool is empty. */
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
