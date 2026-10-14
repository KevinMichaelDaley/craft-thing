#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/lighting_gpu.h"

_Static_assert(sizeof(fr_renderer_point_light_t) == 32,
               "Point light GPU layout must be two vec4 values");
_Static_assert(sizeof(fr_renderer_light_field_t) == 64,
               "Lighting field GPU layout must be four vec4 values");
#include "renderer_internal.h"

struct fr_lighting_gpu {
  dc_gpu_t *gpu;
  uint32_t width, height;
  VkBuffer parameters, sources, field;
  VkDeviceMemory parameter_memory, source_memory, field_memory;
  void *parameter_mapped, *source_mapped, *field_mapped;
  fr_renderer_compute_t compute;
};
void fr_lighting_gpu_destroy(fr_lighting_gpu_t *l) {
  if (!l)
    return;
  VkDevice d = l->gpu->device;
  fr_renderer_compute_destroy(l->gpu, &l->compute);
  if (l->parameter_mapped)
    vkUnmapMemory(d, l->parameter_memory);
  if (l->source_mapped)
    vkUnmapMemory(d, l->source_memory);
  if (l->field_mapped)
    vkUnmapMemory(d, l->field_memory);
  if (l->parameters)
    vkDestroyBuffer(d, l->parameters, NULL);
  if (l->sources)
    vkDestroyBuffer(d, l->sources, NULL);
  if (l->field)
    vkDestroyBuffer(d, l->field, NULL);
  if (l->parameter_memory)
    vkFreeMemory(d, l->parameter_memory, NULL);
  if (l->source_memory)
    vkFreeMemory(d, l->source_memory, NULL);
  if (l->field_memory)
    vkFreeMemory(d, l->field_memory, NULL);
  free(l);
}
bool fr_lighting_gpu_create(dc_gpu_t *g, fr_occlusion_gpu_t *c,
                            fr_lighting_gpu_t **out, char *err, uint32_t cap) {
  if (out)
    *out = NULL;
  if (!g || !c || !out)
    return false;
  fr_lighting_gpu_t *l = calloc(1, sizeof(*l));
  if (!l)
    return false;
  l->gpu = g;
  uint32_t layout[136] = {0};
  VkDescriptorBufferInfo buffers[4];
  if (!fr_occlusion_gpu_layout(c, &buffers[0], layout))
    goto fail;
  l->width = layout[0];
  l->height = layout[1];
  if (!dc_gpu_make_mapped_buffer(g, sizeof(layout), &l->parameters,
                                 &l->parameter_memory, &l->parameter_mapped,
                                 err, cap) ||
      !dc_gpu_make_mapped_buffer(g, 17u * 32u, &l->sources, &l->source_memory,
                                 &l->source_mapped, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, (VkDeviceSize)l->width * l->height * 64,
                                 &l->field, &l->field_memory, &l->field_mapped,
                                 err, cap))
    goto fail;
  memcpy(l->parameter_mapped, layout, sizeof(layout));
  buffers[1] = (VkDescriptorBufferInfo){l->parameters, 0, sizeof(layout)};
  buffers[2] = (VkDescriptorBufferInfo){l->sources, 0, 17u * 32u};
  buffers[3] = (VkDescriptorBufferInfo){
      l->field, 0, (VkDeviceSize)l->width * l->height * 64};
  if (!fr_renderer_compute_init(g, &l->compute, buffers, 4, 4,
                                "build/shaders/lighting.comp.spv", err, cap))
    goto fail;
  *out = l;
  return true;
fail:
  fr_lighting_gpu_destroy(l);
  return false;
}
bool fr_lighting_gpu_set_sources(fr_lighting_gpu_t *l,
                                 const fr_renderer_point_light_t *points,
                                 uint32_t count, fr_renderer_sun_t sun,
                                 uint32_t budget) {
  if (!l || count > FR_RENDERER_POINT_LIGHT_CAPACITY || (count && !points) ||
      !budget || budget > 65536 || sun.known_sky > 15)
    return false;
  for (uint32_t i = 0; i < count; i++) {
    const float values[8] = {points[i].x,        points[i].y,
                             points[i].height,   points[i].radius,
                             points[i].color[0], points[i].color[1],
                             points[i].color[2], points[i].intensity};
    for (uint32_t k = 0; k < 8; k++)
      if (!isfinite(values[k]))
        return false;
    if (points[i].radius <= 0 || points[i].height < 0 ||
        points[i].intensity < 0)
      return false;
    for (uint32_t k = 0; k < 3; k++)
      if (points[i].color[k] < 0)
        return false;
  }
  float length = 0;
  for (uint32_t k = 0; k < 3; k++) {
    if (!isfinite(sun.direction[k]) || !isfinite(sun.color[k]) ||
        sun.color[k] < 0)
      return false;
    length += sun.direction[k] * sun.direction[k];
  }
  if (!isfinite(sun.intensity) || sun.intensity < 0 ||
      (sun.intensity > 0 && length <= 0) || !isfinite(length))
    return false;
  float *sources = l->source_mapped;
  for (uint32_t i = 0; i < count; i++)
    memcpy(sources + 8 * i, &points[i], 32);
  float inv = length > 0 ? 1 / sqrtf(length) : 0;
  for (uint32_t k = 0; k < 3; k++) {
    sources[8 * count + k] = sun.direction[k] * inv;
    sources[8 * count + 4 + k] = sun.color[k];
  }
  sources[8 * count + 3] = -1;
  sources[8 * count + 7] = sun.intensity;
  uint32_t *p = l->parameter_mapped;
  p[3] = count;
  p[4] = budget;
  p[5] = sun.known_sky;
  return true;
}
bool fr_lighting_gpu_generate(fr_lighting_gpu_t *l, char *err, uint32_t cap) {
  (void)err;
  (void)cap;
  if (!l || !fr_renderer_begin(l->gpu))
    return false;
  dc_gpu_t *g = l->gpu;
  fr_renderer_barrier(
      g->command,
      VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    l->compute.pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          l->compute.pipeline_layout, 0, 1,
                          &l->compute.descriptor, 0, NULL);
  vkCmdDispatch(g->command, (l->width * l->height + 7) / 8, 1, 1);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  return fr_renderer_submit(g);
}
bool fr_lighting_gpu_read(fr_lighting_gpu_t *l, uint32_t x, uint32_t y,
                          fr_renderer_light_field_t *field) {
  if (!l || !field || x >= l->width || y >= l->height)
    return false;
  *field = ((fr_renderer_light_field_t *)l->field_mapped)[y * l->width + x];
  return true;
}
