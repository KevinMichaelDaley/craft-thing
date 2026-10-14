#include "ferrum/renderer/gpu.h"

bool fr_renderer_enable(dc_gpu_t *gpu, char *err, uint32_t cap) {
  (void)gpu;
  (void)err;
  (void)cap;
  return false;
}
bool fr_renderer_set_mode(dc_gpu_t *gpu, fr_renderer_mode_t mode) {
  (void)gpu;
  (void)mode;
  return false;
}
bool fr_renderer_set_sources(dc_gpu_t *gpu,
                             const fr_renderer_point_light_t *points,
                             uint32_t count, fr_renderer_sun_t sun) {
  (void)gpu;
  (void)points;
  (void)count;
  (void)sun;
  return false;
}
bool fr_renderer_render(dc_gpu_t *gpu, char *err, uint32_t cap) {
  (void)gpu;
  (void)err;
  (void)cap;
  return false;
}
