#include <stdio.h>
#include <stdlib.h>

#include "ferrum/renderer/texture_gpu.h"
#include "renderer_internal.h"

_Static_assert(sizeof(fr_appearance_texel_t) == 32,
               "Appearance GPU layout must be two vec4 values");

struct fr_texture_gpu {
  dc_gpu_t *gpu;
  uint32_t capacity, count;
  VkBuffer owners, texels;
  VkDeviceMemory owner_memory, texel_memory;
  void *owner_mapped, *texel_mapped;
  fr_renderer_compute_t compute;
};
static bool texture_error(char *err, uint32_t cap, const char *message) {
  if (err && cap)
    snprintf(err, cap, "%s", message);
  return false;
}
void fr_texture_gpu_destroy(fr_texture_gpu_t *t) {
  if (!t)
    return;
  VkDevice d = t->gpu->device;
  fr_renderer_compute_destroy(t->gpu, &t->compute);
  if (t->owner_mapped)
    vkUnmapMemory(d, t->owner_memory);
  if (t->texel_mapped)
    vkUnmapMemory(d, t->texel_memory);
  if (t->owners)
    vkDestroyBuffer(d, t->owners, NULL);
  if (t->texels)
    vkDestroyBuffer(d, t->texels, NULL);
  if (t->owner_memory)
    vkFreeMemory(d, t->owner_memory, NULL);
  if (t->texel_memory)
    vkFreeMemory(d, t->texel_memory, NULL);
  free(t);
}
bool fr_texture_gpu_create(dc_gpu_t *g, uint32_t capacity,
                           fr_texture_gpu_t **out, char *err, uint32_t cap) {
  if (out)
    *out = NULL;
  if (!g || !out || !capacity)
    return texture_error(err, cap, "Invalid texture capacity");
  fr_texture_gpu_t *t = calloc(1, sizeof(*t));
  if (!t)
    return false;
  t->gpu = g;
  t->capacity = capacity;
  if (!dc_gpu_make_mapped_buffer(g, (VkDeviceSize)capacity * 16, &t->owners,
                                 &t->owner_memory, &t->owner_mapped, err,
                                 cap) ||
      !dc_gpu_make_mapped_buffer(g, (VkDeviceSize)capacity * 32, &t->texels,
                                 &t->texel_memory, &t->texel_mapped, err, cap))
    goto fail;
  VkDescriptorBufferInfo buffers[2] = {
      {t->owners, 0, (VkDeviceSize)capacity * 16},
      {t->texels, 0, (VkDeviceSize)capacity * 32}};
  if (!fr_renderer_compute_init(g, &t->compute, buffers, 2, 4,
                                "build/shaders/texture.comp.spv", err, cap))
    goto fail;
  *out = t;
  return true;
fail:
  fr_texture_gpu_destroy(t);
  return texture_error(err, cap, "Cannot create GPU appearance channel");
}
bool fr_texture_gpu_upload(fr_texture_gpu_t *t,
                           const fr_appearance_owner_t *owners,
                           const uint32_t *styles, uint32_t n) {
  if (!t || !owners || !styles || n > t->capacity)
    return false;
  uint32_t *words = t->owner_mapped;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t material;
    if (!fr_appearance_material(owners[i].domain, owners[i].material,
                                &material) ||
        styles[i] > FR_TEXTURE_BRICK)
      return false;
    words[4 * i] = (uint32_t)(int32_t)(owners[i].x % 1048576);
    words[4 * i + 1] = (uint32_t)(int32_t)(owners[i].y % 1048576);
    words[4 * i + 2] =
        (uint32_t)owners[i].identity ^ (uint32_t)(owners[i].identity >> 32);
    words[4 * i + 3] = material | (styles[i] << 8);
  }
  t->count = n;
  return true;
}
bool fr_texture_gpu_generate(fr_texture_gpu_t *t, char *err, uint32_t cap) {
  if (!t || !fr_renderer_begin(t->gpu))
    return texture_error(err, cap, "Cannot begin texture generation");
  dc_gpu_t *g = t->gpu;
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_HOST_BIT,
                      VK_ACCESS_2_HOST_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    t->compute.pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          t->compute.pipeline_layout, 0, 1,
                          &t->compute.descriptor, 0, NULL);
  vkCmdPushConstants(g->command, t->compute.pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &t->count);
  if (t->count)
    vkCmdDispatch(g->command, (t->count + 63) / 64, 1, 1);
  fr_renderer_barrier(g->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  return fr_renderer_submit(g);
}
bool fr_texture_gpu_read(fr_texture_gpu_t *t, uint32_t index,
                         fr_appearance_texel_t *texel) {
  if (!t || !texel || index >= t->count)
    return false;
  *texel = ((fr_appearance_texel_t *)t->texel_mapped)[index];
  return true;
}
