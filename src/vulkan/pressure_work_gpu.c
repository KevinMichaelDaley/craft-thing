#include <stdio.h>

#include "gpu_internal.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
bool dc_gpu_pressure_work_init(dc_gpu_t *g,char *err,uint32_t cap) {
    VkDeviceSize tiles=((VkDeviceSize)g->width+15u)/16u*((g->height+15u)/16u);
    VkDeviceSize bytes=(4u+tiles)*sizeof(uint32_t);
    if(!dc_gpu_make_mapped_buffer(g,bytes,&g->pressure_work_buffer,
        &g->pressure_work_memory,&g->pressure_work_mapped,err,cap)) return false;
    VkDescriptorSetLayoutBinding binding={0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,
        VK_SHADER_STAGE_COMPUTE_BIT,NULL};
    VkDescriptorSetLayoutCreateInfo set={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount=1,.pBindings=&binding};
    if(vkCreateDescriptorSetLayout(g->device,&set,NULL,&g->pressure_work_set_layout)!=VK_SUCCESS)
        return error(err,cap,"Cannot create pressure work descriptor layout");
    VkDescriptorPoolSize size={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};
    VkDescriptorPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets=1,.poolSizeCount=1,.pPoolSizes=&size};
    if(vkCreateDescriptorPool(g->device,&pool,NULL,&g->pressure_work_pool)!=VK_SUCCESS)
        return error(err,cap,"Cannot create pressure work descriptor pool");
    VkDescriptorSetAllocateInfo alloc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool=g->pressure_work_pool,.descriptorSetCount=1,.pSetLayouts=&g->pressure_work_set_layout};
    if(vkAllocateDescriptorSets(g->device,&alloc,&g->pressure_work_descriptor)!=VK_SUCCESS)
        return error(err,cap,"Cannot allocate pressure work descriptor");
    VkDescriptorBufferInfo buffer={g->pressure_work_buffer,0,bytes};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet=g->pressure_work_descriptor,.descriptorCount=1,
        .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
    vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    g->descriptor_revision++;
    VkDescriptorSetLayout sets[]={g->set_layout,g->pressure_work_set_layout};
    VkPushConstantRange range={VK_SHADER_STAGE_COMPUTE_BIT,0,32};
    VkPipelineLayoutCreateInfo layout={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount=2,.pSetLayouts=sets,.pushConstantRangeCount=1,.pPushConstantRanges=&range};
    if(vkCreatePipelineLayout(g->device,&layout,NULL,&g->pressure_pipeline_layout)!=VK_SUCCESS)
        return error(err,cap,"Cannot create pressure work pipeline layout");
    VkShaderModule module;
    if(!dc_gpu_load_shader_module(g,"build/shaders/pressure_work.comp.spv",&module,err,cap)) return false;
    bool okay=true;
    for(uint32_t phase=0;phase<3&&okay;phase++) {
        VkSpecializationMapEntry entry={0,0,sizeof(phase)};
        VkSpecializationInfo specialization={1,&entry,sizeof(phase),&phase};
        VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main",
                .pSpecializationInfo=&specialization},.layout=g->pressure_pipeline_layout};
        okay=vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,
            &g->pressure_work_pipelines[phase])==VK_SUCCESS;
    }
    vkDestroyShaderModule(g->device,module,NULL);
    return okay||error(err,cap,"Cannot create pressure work pipelines");
}
void dc_gpu_pressure_work_destroy(dc_gpu_t *g) {
    for(uint32_t i=0;i<3;i++) if(g->pressure_work_pipelines[i])
        vkDestroyPipeline(g->device,g->pressure_work_pipelines[i],NULL);
    if(g->pressure_pipeline_layout) vkDestroyPipelineLayout(g->device,g->pressure_pipeline_layout,NULL);
    if(g->pressure_work_pool) vkDestroyDescriptorPool(g->device,g->pressure_work_pool,NULL);
    if(g->pressure_work_set_layout) vkDestroyDescriptorSetLayout(g->device,g->pressure_work_set_layout,NULL);
    if(g->pressure_work_mapped) vkUnmapMemory(g->device,g->pressure_work_memory);
    if(g->pressure_work_buffer) vkDestroyBuffer(g->device,g->pressure_work_buffer,NULL);
    if(g->pressure_work_memory) vkFreeMemory(g->device,g->pressure_work_memory,NULL);
}
static void barrier(dc_gpu_t *g) {
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT|VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dependency);
}
void dc_gpu_record_pressure_work(dc_gpu_t *g) {
    barrier(g);
    VkDescriptorSet sets[]={g->descriptor,g->pressure_work_descriptor};
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        g->pressure_pipeline_layout,0,2,sets,0,NULL);
    uint32_t push[7]={g->width,g->height,0,0,0,0,0};
    vkCmdPushConstants(g->command,g->pressure_pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    for(uint32_t phase=0;phase<3;phase++) {
        vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pressure_work_pipelines[phase]);
        vkCmdDispatch(g->command,phase==1?(g->width+15u)/16u:1u,
            phase==1?(g->height+15u)/16u:1u,1u);
        barrier(g);
    }
}
