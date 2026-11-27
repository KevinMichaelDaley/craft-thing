#ifndef FERRUM_DIAGNOSTIC_SHADER_FIXTURE_H
#define FERRUM_DIAGNOSTIC_SHADER_FIXTURE_H

#include "ferrum/fire/bulk_phase.h"
#include "../../src/vulkan/gpu_internal.h"

enum {
    FIXTURE_BASE_BINDINGS = 32,
    FIXTURE_BINDINGS = FIXTURE_BASE_BINDINGS + 2,
    FIXTURE_THERMAL_BINDING = 36,
    FIXTURE_BULK_BINDING = 37,
    FIXTURE_THERMAL_BYTES = 128
};

static void fixture_release_pipeline(probe_t *p) {
    if (p->empty_bulk) vkDestroyBuffer(p->device, p->empty_bulk, NULL);
    if (p->empty_bulk_memory) vkFreeMemory(p->device, p->empty_bulk_memory, NULL);
    p->empty_bulk = VK_NULL_HANDLE;
    p->empty_bulk_memory = VK_NULL_HANDLE;
    if(p->thermal_control) vkDestroyBuffer(p->device,p->thermal_control,NULL);
    if(p->thermal_control_memory) vkFreeMemory(p->device,p->thermal_control_memory,NULL);
    p->thermal_control=VK_NULL_HANDLE;p->thermal_control_memory=VK_NULL_HANDLE;
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
static int fixture_body_pipeline_with_bulk(probe_t *p, dc_gpu_t *gpu, VkBuffer body,
    const char *path, const uint32_t *indices, uint32_t count, uint32_t phase,
    uint32_t bulk_slots) {
    char err[256];
    if (count > FIXTURE_BASE_BINDINGS)
        return 0;
    fixture_release_pipeline(p);
    uint32_t extended[FIXTURE_BINDINGS];
    memcpy(extended, indices, count * sizeof(*indices));
    extended[count++] = FIXTURE_THERMAL_BINDING;
    extended[count++] = FIXTURE_BULK_BINDING;
    indices = extended;
    void *control = NULL;
    if (!dc_gpu_make_mapped_buffer(gpu, FIXTURE_THERMAL_BYTES, &p->thermal_control,
                                   &p->thermal_control_memory, &control, err, sizeof(err)))
        return 0;
    memset(control, 0, FIXTURE_THERMAL_BYTES);
    vkUnmapMemory(p->device, p->thermal_control_memory);
    VkBuffer bulk = gpu->bulk_phase_buffer;
    if (!bulk && bulk_slots) {
        const VkDeviceSize bytes = (VkDeviceSize)bulk_slots * sizeof(fr_bulk_phase_chunk_t);
        void *empty = NULL;
        if (!dc_gpu_make_mapped_buffer(gpu, bytes, &p->empty_bulk, &p->empty_bulk_memory,
                                       &empty, err, sizeof(err))) return 0;
        memset(empty, 0, bytes);
        vkUnmapMemory(p->device, p->empty_bulk_memory);
        bulk = p->empty_bulk;
    }
    printf("minimal pipeline %s phase%u\n", path, phase);
    if (!dc_gpu_load_shader_module(gpu, path, &p->shader, err, sizeof(err))) {
        printf("shader failure: %s\n", err);
        return 0;
    }
    VkDescriptorSetLayoutBinding bindings[FIXTURE_BINDINGS] = {0};
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
    VkDescriptorBufferInfo buffers[FIXTURE_BINDINGS];
    VkWriteDescriptorSet writes[FIXTURE_BINDINGS];
    uint32_t write_count = 0;
    for (unsigned i = 0; i < count; i++) {
        bool is_body = indices[i] == 4u;
        buffers[i] = (VkDescriptorBufferInfo){
            is_body ? body : p->buffer, 0,
            is_body ? dc_gpu_body_storage_bytes(gpu) : 39u * sizeof(uint32_t)};
        if (indices[i] == FIXTURE_THERMAL_BINDING)
            buffers[i] = (VkDescriptorBufferInfo){p->thermal_control, 0, FIXTURE_THERMAL_BYTES};
        if (indices[i] == FIXTURE_BULK_BINDING)
            buffers[i] = (VkDescriptorBufferInfo){bulk, 0, VK_WHOLE_SIZE};
        if (!buffers[i].buffer) continue;
        writes[write_count++] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = p->descriptor,
            .dstBinding = indices[i],
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i]};
    }
    vkUpdateDescriptorSets(p->device, write_count, writes, 0, NULL);
    return 1;
}

static inline int fixture_body_pipeline(probe_t *p, dc_gpu_t *gpu, VkBuffer body,
    const char *path, const uint32_t *indices, uint32_t count, uint32_t phase) {
    return fixture_body_pipeline_with_bulk(p, gpu, body, path, indices, count, phase, 0);
}
#endif
