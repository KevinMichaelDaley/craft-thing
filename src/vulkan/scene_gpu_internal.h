#ifndef FR_SCENE_GPU_INTERNAL_H
#define FR_SCENE_GPU_INTERNAL_H

#include "body_address_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "renderer_internal.h"

enum { SCENE_TEXTURE=2, SCENE_OPTICAL=3, SCENE_COMPOSE=5, SCENE_PHASES=10, SCENE_BUFFERS=5 };
struct fr_scene_gpu {
  fr_renderer_mode_t mode;
  uint32_t light_revision, point_count;
  dc_chunk_coord_t source_origin, last_origin;
  fr_renderer_point_light_t points[16];
  fr_renderer_sun_t sun;
  fr_occlusion_gpu_t *occlusion;
  fr_lighting_gpu_t *lighting;
  fr_renderer_compute_t compute;
  VkPipeline phases[SCENE_PHASES];
  VkBuffer buffers[SCENE_BUFFERS];
  VkDeviceMemory memory[SCENE_BUFFERS];
  void *mapped[SCENE_BUFFERS];
  dc_gpu_address_buffer_t fire_coverage;
  uint32_t fire_view_x, fire_view_y, fire_view_width, fire_view_height;
  bool fire_coverage_ready;
};

#endif
