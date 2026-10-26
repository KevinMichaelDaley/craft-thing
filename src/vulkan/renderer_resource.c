#include <stdio.h>

#include "renderer_internal.h"

bool fr_renderer_compute_init_subgroup(dc_gpu_t *g, fr_renderer_compute_t *c,
                              const VkDescriptorBufferInfo *buffers, uint32_t n,
                              uint32_t push_bytes, const char *shader,
                              uint32_t subgroup,char *err, uint32_t cap) {
  if (!g || !c || !buffers || !n || n > 36)
    return false;
  VkDescriptorSetLayoutBinding bindings[36] = {0};
  for (uint32_t i = 0; i < n; i++)
    bindings[i] =
        (VkDescriptorSetLayoutBinding){i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                       VK_SHADER_STAGE_COMPUTE_BIT, NULL};
  VkDescriptorSetLayoutCreateInfo layout = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = n,
      .pBindings = bindings};
  if (vkCreateDescriptorSetLayout(g->device, &layout, NULL, &c->layout) !=
      VK_SUCCESS)
    return false;
  VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, n};
  VkDescriptorPoolCreateInfo pool = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &size};
  if (vkCreateDescriptorPool(g->device, &pool, NULL, &c->pool) != VK_SUCCESS)
    return false;
  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = c->pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &c->layout};
  if (vkAllocateDescriptorSets(g->device, &alloc, &c->descriptor) != VK_SUCCESS)
    return false;
  VkWriteDescriptorSet writes[36] = {0};
  for (uint32_t i = 0; i < n; i++)
    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = c->descriptor,
        .dstBinding = i,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffers[i]};
  vkUpdateDescriptorSets(g->device, n, writes, 0, NULL);
    g->descriptor_revision++;
  VkPushConstantRange range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes};
  VkPipelineLayoutCreateInfo pl = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &c->layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &range};
  if (vkCreatePipelineLayout(g->device, &pl, NULL, &c->pipeline_layout) !=
      VK_SUCCESS)
    return false;
  VkShaderModule module;
  if (!dc_gpu_load_shader_module(g, shader, &module, err, cap))
    return false;
  VkComputePipelineCreateInfo pi = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main"},
      .layout = c->pipeline_layout};
  VkPipelineShaderStageRequiredSubgroupSizeCreateInfo required={
      .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
      .requiredSubgroupSize=subgroup};
  if(subgroup) pi.stage.pNext=&required;
  VkResult result = vkCreateComputePipelines(g->device, VK_NULL_HANDLE, 1, &pi,
                                             NULL, &c->pipeline);
  vkDestroyShaderModule(g->device, module, NULL);
  return result == VK_SUCCESS;
}
bool fr_renderer_compute_init(dc_gpu_t *g,fr_renderer_compute_t *c,
    const VkDescriptorBufferInfo *buffers,uint32_t n,uint32_t push_bytes,
    const char *shader,char *err,uint32_t cap) {
  return fr_renderer_compute_init_subgroup(g,c,buffers,n,push_bytes,shader,0u,err,cap);
}
void fr_renderer_compute_destroy(dc_gpu_t *g, fr_renderer_compute_t *c) {
  if (c->pipeline)
    vkDestroyPipeline(g->device, c->pipeline, NULL);
  if (c->pipeline_layout)
    vkDestroyPipelineLayout(g->device, c->pipeline_layout, NULL);
  if (c->pool)
    vkDestroyDescriptorPool(g->device, c->pool, NULL);
  if (c->layout)
    vkDestroyDescriptorSetLayout(g->device, c->layout, NULL);
}
void fr_renderer_barrier(VkCommandBuffer command, VkPipelineStageFlags2 src,
                         VkAccessFlags2 sa, VkPipelineStageFlags2 dst,
                         VkAccessFlags2 da) {
  VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                              .srcStageMask = src,
                              .srcAccessMask = sa,
                              .dstStageMask = dst,
                              .dstAccessMask = da};
  VkDependencyInfo dep = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                          .memoryBarrierCount = 1,
                          .pMemoryBarriers = &barrier};
  vkCmdPipelineBarrier2(command, &dep);
}
bool fr_renderer_begin(dc_gpu_t *g) {
  if (vkResetCommandBuffer(g->command, 0) != VK_SUCCESS)
    return false;
  VkCommandBufferBeginInfo begin = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  return vkBeginCommandBuffer(g->command, &begin) == VK_SUCCESS;
}
bool fr_renderer_submit(dc_gpu_t *g) {
  if (vkEndCommandBuffer(g->command) != VK_SUCCESS)
    return false;
  VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &g->command};
  return vkQueueSubmit(g->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
         vkQueueWaitIdle(g->queue) == VK_SUCCESS;
}
