#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_internal.h"

bool dc_gpu_mpm_swap_init(dc_gpu_t *g,char *err,uint32_t cap) {
    const char *control=getenv("DC_MPM_PARTICLE_SWAP");
    g->mpm_swap_buffers=control&&strcmp(control,"1")==0;
    if(!g->mpm_swap_buffers) return true;
    VkShaderModule module=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(g,"build/shaders/mpm.comp.spv",&module,err,cap)) return false;
    uint32_t values[]={5u,0u};
    VkSpecializationMapEntry entries[]={{.constantID=2u,.offset=0,.size=4},
        {.constantID=5u,.offset=4,.size=4}};
    VkSpecializationInfo spec={.mapEntryCount=2,.pMapEntries=entries,.dataSize=sizeof(values),.pData=values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main",.pSpecializationInfo=&spec},
        .layout=g->pipeline_layout};
    VkResult result=vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&g->mpm_swap_commit_pipeline);
    vkDestroyShaderModule(g->device,module,NULL);
    if(result!=VK_SUCCESS&&err&&cap) snprintf(err,cap,"Cannot create metadata-only granular commit");
    return result==VK_SUCCESS;
}

void dc_gpu_mpm_swap_descriptors(dc_gpu_t *g) {
    if(!g->mpm_swap_buffers||g->mpm_swap_descriptor_revision==g->descriptor_revision) return;
    VkCopyDescriptorSet copies[34];uint32_t count=0;
    for(uint32_t binding=0;binding<36;binding++) {
        if(binding==19u||binding==22u) continue;
        copies[count++]=(VkCopyDescriptorSet){.sType=VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
            .srcSet=g->descriptor,.srcBinding=binding,.dstSet=g->mpm_swapped_descriptor,
            .dstBinding=binding,.descriptorCount=1};
    }
    VkDeviceSize bytes=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_MPM_PARTICLES_PER_CHUNK*sizeof(dc_mpm_particle_t);
    VkDescriptorBufferInfo buffers[]={{g->mpm_output_buffer,0,bytes},{g->particle_buffer,0,bytes}};
    VkWriteDescriptorSet writes[2];
    for(uint32_t i=0;i<2;i++) writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet=g->mpm_swapped_descriptor,.dstBinding=i?22u:19u,.descriptorCount=1,
        .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[i]};
    vkUpdateDescriptorSets(g->device,2,writes,count,copies);
    g->mpm_swap_descriptor_revision=g->descriptor_revision;
}
