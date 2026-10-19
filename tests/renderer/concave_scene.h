#ifndef FR_RENDERER_CONCAVE_SCENE_H
#define FR_RENDERER_CONCAVE_SCENE_H

#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/gpu.h"

enum { RENDERER_WIDTH = 480, RENDERER_HEIGHT = 270 };
static bool renderer_concave_scene_sized(dc_gpu_t *gpu, unsigned width, unsigned height, char *err, uint32_t cap) {
  unsigned columns = (width + 63u) / 64u, rows = (height + 63u) / 64u;
  dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
  if (!chunk)
    return false;
  bool okay = true;
  for (unsigned cy = 0; cy < rows && okay; cy++)
    for (unsigned cx = 0; cx < columns && okay; cx++) {
      memset(chunk, 0, sizeof(*chunk));
      chunk->coord = (dc_chunk_coord_t){cx, cy};
      for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++) {
          unsigned wx = cx * 64 + x, wy = cy * 64 + y,
                   material = DC_MATERIAL_AIR;
          if (wy == height - 26u && wx < width)
            material = DC_MATERIAL_STONE;
          else if (wx >= 180 && wx < 260 && wy >= 30 && wy < 94 &&
                   (wx < 186 || wx >= 254 || wy >= 88))
            material = DC_MATERIAL_WOOD;
          else if (wx >= 198 && wx < 270 && wy >= 115 && wy < 163 &&
                   (wx < 204 || (wy - 115) % 6 < 3))
            material = wx < 210 ? DC_MATERIAL_STONE : DC_MATERIAL_DENSE_WOOD;
          chunk->cells[y * 64 + x].material = material;
        }
      okay = dc_gpu_upload_chunk(gpu, cy * columns + cx, chunk, err, cap) &&
             dc_gpu_set_page(gpu, cx, cy, cy * columns + cx, err, cap);
    }
  free(chunk);
  return okay && dc_gpu_set_fluid_interval(gpu, 2) &&
         dc_gpu_enable_automatic_components(gpu) &&
         dc_gpu_tick_step(gpu, err, cap) && fr_renderer_enable(gpu, err, cap);
}
static bool renderer_concave_scene(dc_gpu_t *gpu, char *err, uint32_t cap) {
  return renderer_concave_scene_sized(gpu, RENDERER_WIDTH, RENDERER_HEIGHT, err, cap);
}
static bool renderer_four_lights(dc_gpu_t *gpu, uint32_t count) {
  fr_renderer_point_light_t points[16];
  for (uint32_t i = 0; i < 16; i++) {
    points[i] = (fr_renderer_point_light_t){.x = 80.f + (i % 4) * 110,
                                            .y = 45.f + (i / 4) * 55,
                                            .height = 18,
                                            .radius = 300,
                                            .color = {i % 3 == 0 ? 1.f : .3f,
                                                      i % 3 == 1 ? 1.f : .35f,
                                                      i % 3 == 2 ? 1.f : .3f},
                                            .intensity = 1800};
  }
  points[0].x = 220;
  points[0].y = 55;
  fr_renderer_sun_t sun = {.direction = {-.3f, -.8f, .5f},
                           .color = {1.f, .94f, .8f},
                           .intensity = 1.4f,
                           .known_sky = FR_SKY_TOP};
  return fr_renderer_set_sources(gpu, points, count, sun);
}

#endif
