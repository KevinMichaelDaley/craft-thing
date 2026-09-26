#ifndef DUNGEONCRAFT_GPU_H
#define DUNGEONCRAFT_GPU_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

#define DC_GPU_CHUNK_SLOTS 64u

typedef struct dc_gpu dc_gpu_t;

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

/** Render resident chunks through the GPU page table. */
bool dc_gpu_render_chunks(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Paint a material into the resident chunk atlas through a compute dispatch. */
bool dc_gpu_paint_material(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t radius, uint16_t material,
                           char *err_buf, uint32_t err_cap);

/** Present the current cell buffer to the Vulkan window. */
bool dc_gpu_present(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap);

/** Copy completed GPU cell values into caller-owned memory. */
bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err_buf, uint32_t err_cap);

/** Destroy the context and its Vulkan resources. */
void dc_gpu_destroy(dc_gpu_t *gpu);

#endif
