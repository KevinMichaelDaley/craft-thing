#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/renderer/light_transport.h"
#include "ferrum/renderer/lighting_gpu.h"
#include "renderer_internal.h"
#include "thermal_gpu_state.h"

_Static_assert(sizeof(fr_renderer_point_light_t) == 32, "Point light layout");
_Static_assert(sizeof(fr_renderer_light_field_t) == 64, "Light field layout");

struct fr_lighting_gpu {
  dc_gpu_t *gpu;
  uint32_t width, height, tile_width, tile_height;
  VkBuffer parameters, sources, field, fire_tiles, work;
  VkDeviceMemory parameter_memory, source_memory, field_memory, fire_tile_memory,
      work_memory;
  void *parameter_mapped, *source_mapped, *field_mapped, *work_mapped;
  fr_renderer_compute_t compute;
};

void fr_lighting_gpu_invalidate_sources(fr_lighting_gpu_t *l) {
  if (l) ((uint32_t *)l->parameter_mapped)[FR_TRANSPORT_SOURCE_REVISION]++;
}
uint32_t fr_lighting_gpu_source_builds(const fr_lighting_gpu_t *l) {
  return l ? ((const uint32_t *)l->work_mapped)[7] : 0u;
}
void fr_lighting_gpu_destroy(fr_lighting_gpu_t *l) {
  if (!l) return;
  VkDevice d = l->gpu->device;
  fr_renderer_compute_destroy(l->gpu, &l->compute);
  if (l->parameter_mapped) vkUnmapMemory(d, l->parameter_memory);
  if (l->source_mapped) vkUnmapMemory(d, l->source_memory);
  if (l->field_mapped) vkUnmapMemory(d, l->field_memory);
  if (l->work_mapped) vkUnmapMemory(d, l->work_memory);
  if (l->parameters) vkDestroyBuffer(d, l->parameters, NULL);
  if (l->sources) vkDestroyBuffer(d, l->sources, NULL);
  if (l->field) vkDestroyBuffer(d, l->field, NULL);
  if (l->fire_tiles) vkDestroyBuffer(d, l->fire_tiles, NULL);
  if (l->work) vkDestroyBuffer(d, l->work, NULL);
  if (l->parameter_memory) vkFreeMemory(d, l->parameter_memory, NULL);
  if (l->source_memory) vkFreeMemory(d, l->source_memory, NULL);
  if (l->field_memory) vkFreeMemory(d, l->field_memory, NULL);
  if (l->fire_tile_memory) vkFreeMemory(d, l->fire_tile_memory, NULL);
  if (l->work_memory) vkFreeMemory(d, l->work_memory, NULL);
  free(l);
}
bool fr_lighting_gpu_create(dc_gpu_t *g, fr_occlusion_gpu_t *c,
                            fr_lighting_gpu_t **out, char *err, uint32_t cap) {
  if (out) *out = NULL;
  if (!g || !c || !out) return false;
  fr_lighting_gpu_t *l = calloc(1, sizeof(*l));
  if (!l) return false;
  l->gpu = g;
  uint32_t layout[136] = {0};
  VkDescriptorBufferInfo buffers[7];
  if (!fr_occlusion_gpu_layout(c, &buffers[0], layout) ||
      !fr_occlusion_input_binding(c, &buffers[0])) goto fail;
  l->width = layout[0]; l->height = layout[1];
  l->tile_width = (l->width + FR_TRANSPORT_FIRE_TILE - 1u) / FR_TRANSPORT_FIRE_TILE;
  l->tile_height = (l->height + FR_TRANSPORT_FIRE_TILE - 1u) / FR_TRANSPORT_FIRE_TILE;
  if (!l->width || !l->height ||
      (uint64_t)l->width * l->height > UINT32_MAX / FR_RENDERER_FIELD_BYTES ||
      (uint64_t)l->tile_width * l->tile_height > UINT32_MAX / 32u) goto fail;
  VkDeviceSize field_bytes = (VkDeviceSize)l->width * l->height * FR_RENDERER_FIELD_BYTES;
  VkDeviceSize tile_bytes = (VkDeviceSize)l->tile_width * l->tile_height * 32u;
  if (!dc_gpu_make_mapped_buffer(g, 16u * sizeof(uint32_t), &l->parameters,
                                 &l->parameter_memory, &l->parameter_mapped, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, 17u * sizeof(fr_renderer_point_light_t),
                                 &l->sources, &l->source_memory, &l->source_mapped, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, field_bytes, &l->field,
                                 &l->field_memory, &l->field_mapped, err, cap) ||
      !dc_gpu_make_device_buffer(g, tile_bytes, &l->fire_tiles,
                                  &l->fire_tile_memory, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, 8u * sizeof(uint32_t), &l->work,
                                 &l->work_memory, &l->work_mapped, err, cap)) goto fail;
  memset(l->work_mapped, 0, 8u * sizeof(uint32_t));
  uint32_t *parameters = l->parameter_mapped;
  memset(parameters, 0, 16u * sizeof(uint32_t));
  parameters[0] = l->width; parameters[1] = l->height;
  parameters[2] = l->tile_width; parameters[3] = l->tile_height;
  buffers[1] = (VkDescriptorBufferInfo){l->parameters, 0, 16u * sizeof(uint32_t)};
  buffers[2] = (VkDescriptorBufferInfo){l->sources, 0, 17u * sizeof(fr_renderer_point_light_t)};
  buffers[3] = (VkDescriptorBufferInfo){l->field, 0, field_bytes};
  buffers[4] = (VkDescriptorBufferInfo){l->fire_tiles, 0, tile_bytes};
  buffers[5] = (VkDescriptorBufferInfo){g->page_buffer, 0,
      (VkDeviceSize)g->page_width * g->page_height * sizeof(uint32_t)};
  buffers[6] = (VkDescriptorBufferInfo){l->work, 0, 8u * sizeof(uint32_t)};
  if (!fr_renderer_compute_init(g, &l->compute, buffers, 7, 4,
                                "build/shaders/lighting.comp.spv", err, cap)) goto fail;
  *out = l; return true;
fail:
  fr_lighting_gpu_destroy(l); return false;
}
bool fr_lighting_gpu_set_sources(fr_lighting_gpu_t *l,
                                 const fr_renderer_point_light_t *points,
                                 uint32_t count, fr_renderer_sun_t sun,
                                 uint32_t budget) {
  if (!l || count > FR_RENDERER_POINT_LIGHT_CAPACITY ||
      (count && !points) || !budget || budget > 65536u || sun.known_sky > 15u) return false;
  for (uint32_t i = 0; i < count; i++) {
    const float *values = (const float *)&points[i];
    for (uint32_t k = 0; k < 8u; k++) if (!isfinite(values[k])) return false;
    if (points[i].radius <= 0.f || points[i].height < 0.f ||
        points[i].intensity < 0.f) return false;
    for (uint32_t k = 0; k < 3u; k++) if (points[i].color[k] < 0.f) return false;
  }
  float length = 0.f;
  for (uint32_t k = 0; k < 3u; k++) {
    if (!isfinite(sun.direction[k]) || !isfinite(sun.color[k]) ||
        sun.color[k] < 0.f) return false;
    length += sun.direction[k] * sun.direction[k];
  }
  if (!isfinite(sun.intensity) || sun.intensity < 0.f ||
      (sun.intensity > 0.f && length <= 0.f) || !isfinite(length)) return false;
  if (count) memcpy(l->source_mapped, points, count * sizeof(*points));
  float *sun_record = (float *)l->source_mapped + FR_RENDERER_POINT_LIGHT_CAPACITY * 8u;
  float inv = length > 0.f ? 1.f / sqrtf(length) : 0.f;
  for (uint32_t k = 0; k < 3u; k++) {
    sun_record[k] = sun.direction[k] * inv;
    sun_record[4u + k] = sun.color[k];
  }
  sun_record[3] = 0.f; sun_record[7] = sun.intensity;
  uint32_t *p = l->parameter_mapped;
  p[4] = count; p[5] = budget; p[6] = sun.known_sky;
  p[FR_TRANSPORT_SOURCE_REVISION]++;
  return true;
}
void fr_lighting_record(fr_lighting_gpu_t *l, VkBuffer indirect, VkDeviceSize offset) {
  dc_gpu_t *g = l->gpu;
  VkDeviceAddress fire = g->thermal ? g->thermal->emission.address : 0u;
  memcpy((uint32_t *)l->parameter_mapped + FR_TRANSPORT_FIRE_ADDRESS_WORD,
         &fire, sizeof(fire));
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_READ_BIT |
                      VK_ACCESS_2_TRANSFER_WRITE_BIT);
  if (indirect) {
    VkBufferCopy copy = {offset, 0, sizeof(uint32_t)};
    vkCmdCopyBuffer(g->command, indirect, l->work, 1, &copy);
  } else vkCmdFillBuffer(g->command, l->work, 0, sizeof(uint32_t), 1u);
  fr_renderer_barrier(g->command,
      VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE, l->compute.pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          l->compute.pipeline_layout, 0, 1, &l->compute.descriptor, 0, NULL);
  uint32_t phase = 2u;
  vkCmdPushConstants(g->command, l->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(phase), &phase);
  vkCmdDispatch(g->command, 1, 1, 1);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                      VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  phase = 0u;
  vkCmdPushConstants(g->command, l->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(phase), &phase);
  vkCmdDispatchIndirect(g->command, l->work, sizeof(uint32_t));
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  phase = 1u;
  vkCmdPushConstants(g->command, l->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(phase), &phase);
  vkCmdDispatchIndirect(g->command, l->work, 4u * sizeof(uint32_t));
}
bool fr_lighting_gpu_generate(fr_lighting_gpu_t *l, char *err, uint32_t cap) {
  (void)err; (void)cap;
  if (!l || !fr_renderer_begin(l->gpu)) return false;
  dc_gpu_t *g = l->gpu;
  fr_lighting_record(l, VK_NULL_HANDLE, 0);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  return fr_renderer_submit(g);
}
bool fr_lighting_gpu_read(fr_lighting_gpu_t *l, uint32_t x, uint32_t y,
                          fr_renderer_light_field_t *field) {
  if (!l || !field || x >= l->width || y >= l->height) return false;
  const uint32_t *packed = (const uint32_t *)l->field_mapped + (y * l->width + x) * 6u;
  memset(field, 0, sizeof(*field));
  for (uint32_t color = 0; color < 3u; color++) {
    memcpy(&field->l0[color], &packed[color], sizeof(float));
    float *moments[3] = {field->lx, field->ly, field->lz};
    for (uint32_t axis = 0; axis < 3u; axis++) {
      int32_t component = (int32_t)((packed[3u + color] >> (axis * 10u)) & 1023u);
      if (component >= 512) component -= 1024;
      moments[axis][color] = (float)component / 511.f * field->l0[color];
    }
  }
  return true;
}
bool fr_lighting_field_binding(fr_lighting_gpu_t *l, VkDescriptorBufferInfo *buffer) {
  if (!l || !buffer) return false;
  *buffer = (VkDescriptorBufferInfo){l->field, 0,
      (VkDeviceSize)l->width * l->height * FR_RENDERER_FIELD_BYTES};
  return true;
}
