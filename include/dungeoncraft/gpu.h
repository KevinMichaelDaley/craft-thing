#ifndef DUNGEONCRAFT_GPU_H
#define DUNGEONCRAFT_GPU_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

#define DC_GPU_CHUNK_SLOTS 64u

typedef struct {
    int32_t x_fp, y_fp;
    int32_t vx_fp, vy_fp;
    uint32_t width, height;
    uint32_t id, active;
} dc_gpu_body_t;

typedef struct dc_gpu dc_gpu_t;

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
} dc_gpu_transfer_t;

/** Create a headless Vulkan compute context and a width-by-height cell buffer. */
bool dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                   const char *shader_path, char *err_buf, uint32_t err_cap);

/** Create the same compute context with an SDL Vulkan window for presentation. */
bool dc_gpu_create_window(dc_gpu_t **out, uint32_t width, uint32_t height,
                          uint32_t window_width, uint32_t window_height,
                          const char *shader_path, char *err_buf, uint32_t err_cap);

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

/** Refresh halos and retry the queued transfer; absent destinations stay pending. */
bool dc_gpu_try_transfer(dc_gpu_t *gpu, dc_gpu_transfer_state_t *state,
                         char *err_buf, uint32_t err_cap);

/** Render resident chunks through the GPU page table. */
bool dc_gpu_render_chunks(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Paint a material into the resident chunk atlas through a compute dispatch. */
bool dc_gpu_paint_material(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t radius, uint16_t material,
                           char *err_buf, uint32_t err_cap);

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

/** Submit rigid then fluid/sand probes and capture GPU timings and handoffs.
 * Fluid and sand probes verify ordering until their simulation shaders land. */
bool dc_gpu_tick_capture(dc_gpu_t *gpu, dc_gpu_tick_capture_t *capture,
                         char *err_buf, uint32_t err_cap);

/** Read the completed body state for tests or persistence. */
bool dc_gpu_read_body(dc_gpu_t *gpu, dc_gpu_body_t *body,
                      char *err_buf, uint32_t err_cap);

/** Present the current cell buffer to the Vulkan window. */
bool dc_gpu_present(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Copy completed GPU cell values into caller-owned memory. */
bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err_buf, uint32_t err_cap);

/** Destroy the context and its Vulkan resources. */
void dc_gpu_destroy(dc_gpu_t *gpu);

#endif
