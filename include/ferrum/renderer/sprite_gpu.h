#ifndef FERRUM_RENDERER_SPRITE_GPU_H
#define FERRUM_RENDERER_SPRITE_GPU_H

#include "dungeoncraft/gpu.h"

#define FR_RENDERER_SPRITES 4u
/** Immutable texel: packed linear RGBA8 and canonical 40-column pixel index+1.
 * Zero source/alpha is transparent. Physical/reactive state stays in the body. */
typedef struct { uint32_t rgba, source; } fr_renderer_sprite_pixel_t;
typedef struct {
    uint32_t body_id,generation,offset,width,height,flip;
    uint32_t rest_origin_x,rest_origin_y;
    int32_t origin_x,origin_y;
} fr_renderer_sprite_t;
/** Cold upload of immutable animation atlas. Replaces only appearance data;
 * no matter, reaction state, inventory, movement or action events are changed.
 * Set poses after upload. Empty count detaches all poses. */
bool fr_renderer_sprite_atlas(dc_gpu_t *gpu,
    const fr_renderer_sprite_pixel_t *pixels,uint32_t count,char *err,uint32_t cap);
/** Publish generation-bearing body poses at a completed command boundary.
 * The shader resolves current body transforms without CPU pose readback. */
bool fr_renderer_sprite_poses(dc_gpu_t *gpu,const fr_renderer_sprite_t *poses,
    uint32_t count,char *err,uint32_t cap);

#endif
