#ifndef FERRUM_DIAGNOSTIC_SHADER_FIXTURE_H
#define FERRUM_DIAGNOSTIC_SHADER_FIXTURE_H

#include "../../src/vulkan/gpu_internal.h"

static void fixture_release_pipeline(probe_t *p) {
    if (p->pipeline)
        vkDestroyPipeline(p->device, p->pipeline, NULL);
    if (p->shader)
        vkDestroyShaderModule(p->device, p->shader, NULL);
    if (p->layout)
        vkDestroyPipelineLayout(p->device, p->layout, NULL);
    if (p->descriptor_pool)
        vkDestroyDescriptorPool(p->device, p->descriptor_pool, NULL);
    if (p->descriptor_layout)
        vkDestroyDescriptorSetLayout(p->device, p->descriptor_layout, NULL);
    p->pipeline = VK_NULL_HANDLE;
    p->shader = VK_NULL_HANDLE;
    p->layout = VK_NULL_HANDLE;
    p->descriptor_pool = VK_NULL_HANDLE;
    p->descriptor_layout = VK_NULL_HANDLE;
}
static int fixture_body_pipeline(probe_t *p, dc_gpu_t *gpu, VkBuffer body,
                                 const char *path, const uint32_t *indices,
                                 uint32_t count, uint32_t phase) {
    char err[256];
    if (count > 32u)
        return 0;
    fixture_release_pipeline(p);
    printf("minimal pipeline %s phase%u\n", path, phase);
    if (!dc_gpu_load_shader_module(gpu, path, &p->shader, err, sizeof(err))) {
        printf("shader failure: %s\n", err);
        return 0;
    }
    VkDescriptorSetLayoutBinding bindings[32] = {0};
    for (unsigned i = 0; i < count; i++)
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = indices[i],
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo descriptor = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = count,
        .pBindings = bindings};
    if (!check(vkCreateDescriptorSetLayout(p->device, &descriptor, NULL,
                                           &p->descriptor_layout),
               "descriptor layout"))
        return 0;
    VkPushConstantRange range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                 .size = 32};
    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &p->descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range};
    if (!check(vkCreatePipelineLayout(p->device, &layout, NULL, &p->layout),
               "pipeline layout"))
        return 0;
    VkSpecializationMapEntry entry = {.constantID = 0, .size = sizeof(phase)};
    VkSpecializationInfo specialization = {.mapEntryCount = 1,
                                           .pMapEntries = &entry,
                                           .dataSize = sizeof(phase),
                                           .pData = &phase};
    VkComputePipelineCreateInfo pipeline = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = p->shader,
                  .pName = "main",
                  .pSpecializationInfo =
                      phase == UINT32_MAX ? NULL : &specialization},
        .layout = p->layout};
#ifdef DC_FIXTURE_WATER_COLOR
    uint32_t water_values[2]={phase,1u};
    VkSpecializationMapEntry water_entries[2]={{.constantID=0,.size=4},
        {.constantID=2,.offset=4,.size=4}};
    VkSpecializationInfo water_specialization={.mapEntryCount=2,.pMapEntries=water_entries,
        .dataSize=sizeof(water_values),.pData=water_values};
    pipeline.stage.pSpecializationInfo=&water_specialization;
#ifdef DC_FIXTURE_COLOR_CACHE
    uint32_t cache_values[3]={phase,1u,1u};
    VkSpecializationMapEntry cache_entries[3]={{.constantID=0,.size=4},
        {.constantID=2,.offset=4,.size=4},{.constantID=4,.offset=8,.size=4}};
    VkSpecializationInfo cache_specialization={.mapEntryCount=3,.pMapEntries=cache_entries,
        .dataSize=sizeof(cache_values),.pData=cache_values};
    pipeline.stage.pSpecializationInfo=&cache_specialization;
#endif
#endif
    if (!check(vkCreateComputePipelines(p->device, VK_NULL_HANDLE, 1, &pipeline,
                                        NULL, &p->pipeline),
               "pipeline create"))
        return 0;
    VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count};
    VkDescriptorPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &size};
    if (!check(
            vkCreateDescriptorPool(p->device, &pool, NULL, &p->descriptor_pool),
            "descriptor pool"))
        return 0;
    VkDescriptorSetAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = p->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &p->descriptor_layout};
    if (!check(vkAllocateDescriptorSets(p->device, &allocation, &p->descriptor),
               "descriptor allocate"))
        return 0;
    VkDescriptorBufferInfo buffers[32];
    VkWriteDescriptorSet writes[32];
    for (unsigned i = 0; i < count; i++) {
        bool is_body = indices[i] == 4u;
        buffers[i] = (VkDescriptorBufferInfo){
            is_body ? body : p->buffer, 0,
            is_body ? dc_gpu_body_storage_bytes(gpu) : 39u * sizeof(uint32_t)};
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = p->descriptor,
            .dstBinding = indices[i],
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i]};
    }
    vkUpdateDescriptorSets(p->device, count, writes, 0, NULL);
    return 1;
}
#endif
