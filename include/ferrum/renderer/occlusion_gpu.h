#ifndef FERRUM_RENDERER_OCCLUSION_GPU_H
#define FERRUM_RENDERER_OCCLUSION_GPU_H

#include "dungeoncraft/gpu.h"

typedef struct fr_occlusion_gpu fr_occlusion_gpu_t;
/** Opacity extrema and known residency for a conservative hierarchy node. */
typedef struct {
  float minimum, maximum;
  uint32_t known, reserved;
} fr_occlusion_node_t;

/** Allocate a fixed one-pixel-preserving hierarchy, separate from appearance.
 */
bool fr_occlusion_gpu_create(dc_gpu_t *gpu, uint32_t width, uint32_t height,
                             fr_occlusion_gpu_t **out, char *err, uint32_t cap);
/** Release the hierarchy after submitted work completes. */
void fr_occlusion_gpu_destroy(fr_occlusion_gpu_t *occlusion);
/** Explicit reference/import input; unknown cells are conservative occluders.
 */
bool fr_occlusion_gpu_upload(fr_occlusion_gpu_t *occlusion,
                             const float *opacity, const uint32_t *known);
/** Build each hierarchy level with one independent invocation per node. */
bool fr_occlusion_gpu_generate(fr_occlusion_gpu_t *occlusion, char *err,
                               uint32_t cap);
/** Diagnostic query; never used as ordinary-frame scene copyback. */
bool fr_occlusion_gpu_read(fr_occlusion_gpu_t *occlusion, uint32_t level,
                           uint32_t x, uint32_t y, fr_occlusion_node_t *node);

#endif
