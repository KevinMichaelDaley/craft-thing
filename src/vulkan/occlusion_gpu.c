#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/renderer/occlusion_gpu.h"
#include "renderer_internal.h"

struct fr_occlusion_gpu {
  dc_gpu_t *gpu;
  uint32_t width[32], height[32], offset[32], levels, nodes;
  VkBuffer input, hierarchy;
  VkDeviceMemory input_memory, hierarchy_memory;
  void *input_mapped, *hierarchy_mapped;
  fr_renderer_compute_t compute;
};
static bool occlusion_error(char *err, uint32_t cap, const char *message) {
  if (err && cap)
    snprintf(err, cap, "%s", message);
  return false;
}
void fr_occlusion_gpu_destroy(fr_occlusion_gpu_t *c) {
  if (!c)
    return;
  VkDevice d = c->gpu->device;
  fr_renderer_compute_destroy(c->gpu, &c->compute);
  if (c->input_mapped)
    vkUnmapMemory(d, c->input_memory);
  if (c->hierarchy_mapped)
    vkUnmapMemory(d, c->hierarchy_memory);
  if (c->input)
    vkDestroyBuffer(d, c->input, NULL);
  if (c->hierarchy)
    vkDestroyBuffer(d, c->hierarchy, NULL);
  if (c->input_memory)
    vkFreeMemory(d, c->input_memory, NULL);
  if (c->hierarchy_memory)
    vkFreeMemory(d, c->hierarchy_memory, NULL);
  free(c);
}
bool fr_occlusion_gpu_create(dc_gpu_t *g, uint32_t w, uint32_t h,
                             fr_occlusion_gpu_t **out, char *err,
                             uint32_t cap) {
  if (out)
    *out = NULL;
  if (!g || !out || !w || !h || (uint64_t)w * h > UINT32_MAX / 32u)
    return occlusion_error(err, cap, "Invalid occlusion dimensions");
  fr_occlusion_gpu_t *c = calloc(1, sizeof(*c));
  if (!c)
    return false;
  c->gpu = g;
  do {
    uint32_t l = c->levels++;
    c->width[l] = w;
    c->height[l] = h;
    c->offset[l] = c->nodes;
    c->nodes += w * h;
    if (w == 1 && h == 1)
      break;
    w = (w + 1) / 2;
    h = (h + 1) / 2;
  } while (c->levels < 32);
  VkDeviceSize input_bytes = (VkDeviceSize)c->width[0] * c->height[0] * 8,
               node_bytes = (VkDeviceSize)c->nodes * 16;
  if (!dc_gpu_make_mapped_buffer(g, input_bytes, &c->input, &c->input_memory,
                                 &c->input_mapped, err, cap) ||
      !dc_gpu_make_mapped_buffer(g, node_bytes, &c->hierarchy,
                                 &c->hierarchy_memory, &c->hierarchy_mapped,
                                 err, cap))
    goto fail;
  VkDescriptorBufferInfo buffers[2] = {{c->input, 0, input_bytes},
                                       {c->hierarchy, 0, node_bytes}};
  if (!fr_renderer_compute_init(g, &c->compute, buffers, 2, 24,
                                "build/shaders/occlusion.comp.spv", err, cap))
    goto fail;
  *out = c;
  return true;
fail:
  fr_occlusion_gpu_destroy(c);
  return false;
}
bool fr_occlusion_gpu_upload(fr_occlusion_gpu_t *c, const float *opacity,
                             const uint32_t *known) {
  if (!c || !opacity || !known)
    return false;
  uint32_t count = c->width[0] * c->height[0];
  for (uint32_t i = 0; i < count; i++)
    if (!isfinite(opacity[i]) || opacity[i] < 0 || opacity[i] > 1 ||
        known[i] > 1)
      return false;
  typedef struct {
    float opacity;
    uint32_t known;
  } input_t;
  input_t *inputs = c->input_mapped;
  for (uint32_t i = 0; i < count; i++)
    inputs[i] = (input_t){opacity[i], known[i]};
  return true;
}
bool fr_occlusion_gpu_generate(fr_occlusion_gpu_t *c, char *err, uint32_t cap) {
  if (!c || !fr_renderer_begin(c->gpu))
    return occlusion_error(err, cap, "Cannot begin occlusion generation");
  dc_gpu_t *g = c->gpu;
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    c->compute.pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          c->compute.pipeline_layout, 0, 1,
                          &c->compute.descriptor, 0, NULL);
  for (uint32_t l = 0; l < c->levels; l++) {
    uint32_t p[6] = {c->width[l],
                     c->height[l],
                     c->offset[l],
                     l ? c->offset[l - 1] : 0,
                     l ? c->width[l - 1] : 0,
                     l ? c->height[l - 1] : 0};
    vkCmdPushConstants(g->command, c->compute.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(p), p);
    vkCmdDispatch(g->command, (c->width[l] * c->height[l] + 63) / 64, 1, 1);
    fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  }
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  return fr_renderer_submit(g);
}
bool fr_occlusion_gpu_read(fr_occlusion_gpu_t *c, uint32_t level, uint32_t x,
                           uint32_t y, fr_occlusion_node_t *node) {
  if (!c || !node || level >= c->levels || x >= c->width[level] ||
      y >= c->height[level])
    return false;
  *node = ((fr_occlusion_node_t *)
               c->hierarchy_mapped)[c->offset[level] + y * c->width[level] + x];
  return true;
}

bool fr_occlusion_gpu_layout(fr_occlusion_gpu_t *c,
                             VkDescriptorBufferInfo *buffer, uint32_t *layout) {
  if (!c || !buffer || !layout)
    return false;
  *buffer =
      (VkDescriptorBufferInfo){c->hierarchy, 0, (VkDeviceSize)c->nodes * 16};
  layout[0] = c->width[0];
  layout[1] = c->height[0];
  layout[2] = c->levels;
  for (uint32_t l = 0; l < 32; l++) {
    layout[8 + 4 * l] = c->offset[l];
    layout[9 + 4 * l] = c->width[l];
    layout[10 + 4 * l] = c->height[l];
    layout[11 + 4 * l] = 0;
  }
  return true;
}
