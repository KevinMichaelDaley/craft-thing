#ifndef FERRUM_RENDERER_GPU_H
#define FERRUM_RENDERER_GPU_H

#include "ferrum/renderer/appearance.h"
#include "dungeoncraft/color.h"
#include "ferrum/renderer/lighting_gpu.h"

typedef enum {
  FR_RENDERER_LIT,
  FR_RENDERER_ALBEDO,
  FR_RENDERER_REFERENCE
} fr_renderer_mode_t;

/** Lazily allocate separate appearance, occlusion and lighting channels. */
bool fr_renderer_enable(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Choose lit appearance, unlit textured appearance, or exact physics reference
 * colors. */
bool fr_renderer_set_mode(dc_gpu_t *gpu, fr_renderer_mode_t mode);
/** Edit scene-pixel light inputs without modifying physics or material indices.
 */
bool fr_renderer_set_sources(dc_gpu_t *gpu,
                             const fr_renderer_point_light_t *points,
                             uint32_t count, fr_renderer_sun_t sun);
/** Render current physics coverage through the separated channels without
 * advancing it. */
bool fr_renderer_render(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Diagnostic optical query after rendering; never reads back during simulation. */
bool fr_renderer_read_opacity(dc_gpu_t *gpu,uint32_t x,uint32_t y,
                              fr_occlusion_node_t *node);
/** Set a cosmetic texture preset for a terrain-namespace material without
 * changing it. */
bool fr_renderer_set_material_style(dc_gpu_t *gpu, uint32_t material,
                                    uint32_t style);

/** Read colors at an explicit streaming/checkpoint boundary, never per physics tick. */
bool fr_renderer_download_chunk_colors(dc_gpu_t *gpu,uint32_t slot,
    dc_chunk_colors_t *colors,char *err,uint32_t cap);
/** Snapshot all resident color slots through host staging at a save boundary. */
bool fr_renderer_capture_chunk_colors(dc_gpu_t *gpu,dc_chunk_colors_t *colors,
    uint32_t slots,char *err,uint32_t cap);
/** Publish a validated saved color map into an already loaded physical slot. */
bool fr_renderer_upload_chunk_colors(dc_gpu_t *gpu,uint32_t slot,
    const dc_chunk_colors_t *colors,char *err,uint32_t cap);
#endif
