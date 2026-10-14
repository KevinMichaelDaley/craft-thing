#include <stdio.h>
#include <stdlib.h>

#include "ferrum/renderer/texture_gpu.h"
#include "gpu_internal.h"

struct fr_texture_gpu {
  dc_gpu_t *gpu;
  uint32_t capacity, count;
  VkBuffer owners, texels;
  VkDeviceMemory owner_memory, texel_memory;
  void *owner_mapped, *texel_mapped;
  VkDescriptorSetLayout layout;
  VkDescriptorPool pool;
  VkDescriptorSet descriptor;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
};

static bool texture_error(char *err, uint32_t cap, const char *message) {
  if (err && cap)
    snprintf(err, cap, "%s", message);
  return false;
}

void fr_texture_gpu_destroy(fr_texture_gpu_t *t) {
  if (!t)
    return;
  VkDevice device = t->gpu->device;
  if (t->pipeline)
    vkDestroyPipeline(device, t->pipeline, NULL);
  if (t->pipeline_layout)
    vkDestroyPipelineLayout(device, t->pipeline_layout, NULL);
  if (t->pool)
    vkDestroyDescriptorPool(device, t->pool, NULL);
  if (t->layout)
    vkDestroyDescriptorSetLayout(device, t->layout, NULL);
  if (t->owner_mapped)
    vkUnmapMemory(device, t->owner_memory);
  if (t->texel_mapped)
    vkUnmapMemory(device, t->texel_memory);
  if (t->owners)
    vkDestroyBuffer(device, t->owners, NULL);
  if (t->texels)
    vkDestroyBuffer(device, t->texels, NULL);
  if (t->owner_memory)
    vkFreeMemory(device, t->owner_memory, NULL);
  if (t->texel_memory)
    vkFreeMemory(device, t->texel_memory, NULL);
  free(t);
}

bool fr_texture_gpu_create(dc_gpu_t *gpu, uint32_t n, fr_texture_gpu_t **out,
                           char *err, uint32_t cap) {
  if (out)
    *out = NULL;
  if (!gpu || !out || !n)
    return texture_error(err, cap, "Invalid texture channel capacity");
  fr_texture_gpu_t *t = calloc(1, sizeof(*t));
  if (!t)
    return texture_error(err, cap, "Cannot allocate texture channel");
  t->gpu = gpu;
  t->capacity = n;
  if (!dc_gpu_make_mapped_buffer(gpu, (VkDeviceSize)n * 16, &t->owners,
                                 &t->owner_memory, &t->owner_mapped, err,
                                 cap) ||
      !dc_gpu_make_mapped_buffer(gpu, (VkDeviceSize)n * 32, &t->texels,
                                 &t->texel_memory, &t->texel_mapped, err, cap))
    goto fail;
  VkDescriptorSetLayoutBinding bindings[2] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
       NULL},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
       NULL}};
  VkDescriptorSetLayoutCreateInfo layout = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings};
  if (vkCreateDescriptorSetLayout(gpu->device, &layout, NULL, &t->layout) !=
      VK_SUCCESS)
    goto fail;
  VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
  VkDescriptorPoolCreateInfo pool = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &size};
  if (vkCreateDescriptorPool(gpu->device, &pool, NULL, &t->pool) != VK_SUCCESS)
    goto fail;
  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = t->pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &t->layout};
  if (vkAllocateDescriptorSets(gpu->device, &alloc, &t->descriptor) !=
      VK_SUCCESS)
    goto fail;
  VkDescriptorBufferInfo buffers[2] = {{t->owners, 0, (VkDeviceSize)n * 16},
                                       {t->texels, 0, (VkDeviceSize)n * 32}};
  VkWriteDescriptorSet writes[2] = {0};
  for (uint32_t i = 0; i < 2; i++)
    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = t->descriptor,
        .dstBinding = i,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffers[i]};
  vkUpdateDescriptorSets(gpu->device, 2, writes, 0, NULL);
  VkPushConstantRange range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 4};
  VkPipelineLayoutCreateInfo pl = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &t->layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &range};
  if (vkCreatePipelineLayout(gpu->device, &pl, NULL, &t->pipeline_layout) !=
      VK_SUCCESS)
    goto fail;
  VkShaderModule module;
  if (!dc_gpu_load_shader_module(gpu, "build/shaders/texture.comp.spv", &module,
                                 err, cap))
    goto fail;
  VkComputePipelineCreateInfo pi = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main"},
      .layout = t->pipeline_layout};
  VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
                                             &pi, NULL, &t->pipeline);
  vkDestroyShaderModule(gpu->device, module, NULL);
  if (result != VK_SUCCESS)
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
  if (!t)
    return texture_error(err, cap, "Texture channel is null");
  dc_gpu_t *g = t->gpu;
  if (vkResetCommandBuffer(g->command, 0) != VK_SUCCESS)
    return false;
  VkCommandBufferBeginInfo begin = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(g->command, &begin) != VK_SUCCESS)
    return false;
  VkMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
      .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
  VkDependencyInfo dep = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                          .memoryBarrierCount = 1,
                          .pMemoryBarriers = &barrier};
  vkCmdPipelineBarrier2(g->command, &dep);
  vkCmdBindPipeline(g->command, VK_PIPELINE_BIND_POINT_COMPUTE, t->pipeline);
  vkCmdBindDescriptorSets(g->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          t->pipeline_layout, 0, 1, &t->descriptor, 0, NULL);
  vkCmdPushConstants(g->command, t->pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &t->count);
  if (t->count)
    vkCmdDispatch(g->command, (t->count + 63) / 64, 1, 1);
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
  vkCmdPipelineBarrier2(g->command, &dep);
  if (vkEndCommandBuffer(g->command) != VK_SUCCESS)
    return false;
  VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &g->command};
  return vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
         vkQueueWaitIdle(g->queue) == VK_SUCCESS;
}

bool fr_texture_gpu_read(fr_texture_gpu_t *t, uint32_t index,
                         fr_appearance_texel_t *texel) {
  if (!t || !texel || index >= t->count)
    return false;
  *texel = ((fr_appearance_texel_t *)t->texel_mapped)[index];
  return true;
}
