#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_frontier_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return dc_gpu_make_mapped_buffer(gpu,
        (VkDeviceSize)gpu->slot_capacity * sizeof(uint32_t),
        &gpu->frontier_mask_buffer, &gpu->frontier_mask_memory,
        (void **)&gpu->frontier_mask_mapped, err, cap);
}

void dc_gpu_frontier_destroy(dc_gpu_t *gpu) {
    if (gpu->frontier_pipeline)
        vkDestroyPipeline(gpu->device, gpu->frontier_pipeline, NULL);
    if (gpu->frontier_layout)
        vkDestroyPipelineLayout(gpu->device, gpu->frontier_layout, NULL);
    if (gpu->frontier_pool)
        vkDestroyDescriptorPool(gpu->device, gpu->frontier_pool, NULL);
    if (gpu->frontier_set_layout)
        vkDestroyDescriptorSetLayout(gpu->device, gpu->frontier_set_layout, NULL);
    if (gpu->frontier_mask_mapped)
        vkUnmapMemory(gpu->device, gpu->frontier_mask_memory);
    if (gpu->frontier_mask_buffer)
        vkDestroyBuffer(gpu->device, gpu->frontier_mask_buffer, NULL);
    if (gpu->frontier_mask_memory)
        vkFreeMemory(gpu->device, gpu->frontier_mask_memory, NULL);
}

static bool make_pipeline(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDescriptorSetLayoutBinding bindings[2] = {0};
    for (uint32_t i = 0; i < 2u; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1u;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2u, .pBindings = bindings };
    if (vkCreateDescriptorSetLayout(gpu->device, &layout, NULL,
                                    &gpu->frontier_set_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create wet-frontier descriptor layout");
    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u };
    VkDescriptorPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1u, .poolSizeCount = 1u, .pPoolSizes = &size };
    if (vkCreateDescriptorPool(gpu->device, &pool, NULL,
                               &gpu->frontier_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create wet-frontier descriptor pool");
    VkDescriptorSetAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gpu->frontier_pool, .descriptorSetCount = 1u,
        .pSetLayouts = &gpu->frontier_set_layout };
    if (vkAllocateDescriptorSets(gpu->device, &allocation,
                                 &gpu->frontier_set) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate wet-frontier descriptor set");
    VkPipelineLayoutCreateInfo pipeline_layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1u, .pSetLayouts = &gpu->frontier_set_layout };
    if (vkCreatePipelineLayout(gpu->device, &pipeline_layout, NULL,
                               &gpu->frontier_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create wet-frontier pipeline layout");
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/wet_frontier.comp.spv",
                                   &shader, err, cap)) return false;
    VkComputePipelineCreateInfo pipeline = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader,
            .pName = "main" }, .layout = gpu->frontier_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
        1u, &pipeline, NULL, &gpu->frontier_pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS)
        return error(err, cap, "Cannot create wet-frontier pipeline");
    VkDescriptorBufferInfo buffers[2] = {
        { gpu->chunk_buffer, 0,
          (VkDeviceSize)gpu->slot_capacity * DC_CHUNK_CELLS * sizeof(dc_cell_t) },
        { gpu->frontier_mask_buffer, 0,
          (VkDeviceSize)gpu->slot_capacity * sizeof(uint32_t) } };
    VkWriteDescriptorSet writes[2] = {0};
    for (uint32_t i = 0; i < 2u; ++i) {
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = gpu->frontier_set, .dstBinding = i,
            .descriptorCount = 1u,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i] };
    }
    vkUpdateDescriptorSets(gpu->device, 2u, writes, 0u, NULL);
    return true;
}

bool dc_gpu_wet_edge_masks(dc_gpu_t *gpu, uint32_t *masks, uint32_t mask_capacity,
                           char *err, uint32_t cap) {
    if (!gpu || !masks || mask_capacity < gpu->slot_capacity)
        return error(err, cap, "Invalid wet-frontier mask output");
    if (!gpu->frontier_pipeline && !make_pipeline(gpu, err, cap)) return false;
    size_t bytes = (size_t)gpu->slot_capacity * sizeof(uint32_t);
    memset(gpu->frontier_mask_mapped, 0, bytes);
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset wet-frontier command buffer");
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin wet-frontier command buffer");
    VkMemoryBarrier2 before = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT |
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                         VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1u, .pMemoryBarriers = &before };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->frontier_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->frontier_layout, 0u, 1u, &gpu->frontier_set, 0u, NULL);
    vkCmdDispatch(gpu->command, gpu->slot_capacity, 1u, 1u);
    VkMemoryBarrier2 after = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT };
    dependency.pMemoryBarriers = &after;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end wet-frontier command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1u, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU wet-frontier sample failed");
    memcpy(masks, gpu->frontier_mask_mapped, bytes);
    return true;
}
