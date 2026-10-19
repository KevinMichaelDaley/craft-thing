#ifndef FERRUM_RENDERER_LIGHTING_GPU_H
#define FERRUM_RENDERER_LIGHTING_GPU_H

#include "ferrum/renderer/occlusion_gpu.h"

#define FR_RENDERER_POINT_LIGHT_CAPACITY 16u
enum {
  FR_SKY_LEFT = 1u,
  FR_SKY_RIGHT = 2u,
  FR_SKY_TOP = 4u,
  FR_SKY_BOTTOM = 8u
};
/** Position/height/radius in scene pixels; linear RGB and intensity in relative
 * radiance. */
typedef struct {
  float x, y, height, radius;
  float color[3], intensity;
} fr_renderer_point_light_t;
/** Unit incoming direction and linear radiance; boundary bits explicitly
 * certify sky. */
typedef struct {
  float direction[3], intensity;
  float color[3];
  uint32_t known_sky;
} fr_renderer_sun_t;
/** RGB energy and signed RGB first moments; opposing directions retain energy.
 */
typedef struct {
  float l0[4], lx[4], ly[4], lz[4];
} fr_renderer_light_field_t;
typedef struct fr_lighting_gpu fr_lighting_gpu_t;

/** Allocate a directional field coupled to a conservative opacity hierarchy. */
bool fr_lighting_gpu_create(dc_gpu_t *gpu, fr_occlusion_gpu_t *occlusion,
                            fr_lighting_gpu_t **out, char *err, uint32_t cap);
/** Release field resources before releasing the referenced hierarchy/context.
 */
void fr_lighting_gpu_destroy(fr_lighting_gpu_t *lighting);
/** Explicit light edit. Propagation reach is measured in scene cells;
 * unknown residency blocks transport. */
bool fr_lighting_gpu_set_sources(fr_lighting_gpu_t *lighting,
                                 const fr_renderer_point_light_t *points,
                                 uint32_t count, fr_renderer_sun_t sun,
                                 uint32_t propagation_reach);
/** Inject sky/emitter radiance, propagate directional beams with parallel
 * affine scans, then aggregate deterministic RGB energy and first moments. */
bool fr_lighting_gpu_generate(fr_lighting_gpu_t *lighting, char *err,
                              uint32_t cap);
/** Diagnostic access to HDR moments, independent of material/color. */
bool fr_lighting_gpu_read(fr_lighting_gpu_t *lighting, uint32_t x, uint32_t y,
                          fr_renderer_light_field_t *field);

#endif
