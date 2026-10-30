#include <stdio.h>

#include "gpu_internal.h"

bool dc_gpu_mpm_compaction_pipeline_init(dc_gpu_t *gpu,char *err,uint32_t cap) {
    VkShaderModule module=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/mpm_compact.comp.spv",&module,err,cap)) return false;
    VkSpecializationMapEntry entry={.constantID=0,.size=sizeof(uint32_t)};
    VkResult result=VK_SUCCESS;
    for(uint32_t phase=0;phase<DC_MPM_COMPACTION_PHASES&&result==VK_SUCCESS;phase++) {
        VkSpecializationInfo constants={.mapEntryCount=1,.pMapEntries=&entry,.dataSize=sizeof(phase),.pData=&phase};
        VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=module,.pName="main",.pSpecializationInfo=&constants},.layout=gpu->pipeline_layout};
        result=vkCreateComputePipelines(gpu->device,gpu->pipeline_cache,1,&info,NULL,&gpu->mpm_compaction_pipelines[phase]);
    }
    vkDestroyShaderModule(gpu->device,module,NULL);
    if(result!=VK_SUCCESS&&err&&cap) snprintf(err,cap,"Cannot create MPM compaction pipelines");
    return result==VK_SUCCESS;
}
void dc_gpu_mpm_compaction_pipeline_destroy(dc_gpu_t *gpu) {
    for(uint32_t phase=0;phase<DC_MPM_COMPACTION_PHASES;phase++)
        if(gpu->mpm_compaction_pipelines[phase]) vkDestroyPipeline(gpu->device,gpu->mpm_compaction_pipelines[phase],NULL);
}
static void activity_barrier(dc_gpu_t *gpu,bool indirect) {
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|(indirect?VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT:0),
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|
            (indirect?VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT:0)};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(gpu->command,&dependency);
}
static void dispatch_linear(dc_gpu_t *gpu,uint32_t groups) {
    uint32_t rows=(groups+DC_MPM_DISPATCH_X_LIMIT-1u)/DC_MPM_DISPATCH_X_LIMIT;
    if(rows==0u) rows=1u;
    vkCmdDispatch(gpu->command,(groups+rows-1u)/rows,rows,1u);
}
static void compact_phase(dc_gpu_t *gpu,uint32_t phase,uint32_t push[7],uint32_t groups) {
    push[2]=phase;
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->mpm_compaction_pipelines[phase]);
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,7u*sizeof(uint32_t),push);
    if(phase==4u) vkCmdDispatchIndirect(gpu->command,gpu->mpm_activity_buffer,
        (VkDeviceSize)gpu->mpm_scan_layout.copy_word*sizeof(uint32_t));
    else dispatch_linear(gpu,groups);
    activity_barrier(gpu,phase>=3u);
}
void dc_gpu_record_active_compaction(dc_gpu_t *gpu,bool rigid) {
    const dc_gpu_mpm_scan_layout_t *l=&gpu->mpm_scan_layout;
    uint32_t push[7]={gpu->width,gpu->height,0u,rigid?1u:0u,0u,l->level_words[0],0u};
    compact_phase(gpu,0,push,l->groups);
    for(uint32_t level=0;level<l->levels;level++) {
        push[4]=l->level_counts[level];push[5]=l->level_words[level];
        push[6]=level+1u<l->levels?l->level_words[level+1u]:l->root_word;
        compact_phase(gpu,1,push,(push[4]+DC_MPM_SCAN_LANES-1u)/DC_MPM_SCAN_LANES);
    }
    for(uint32_t level=l->levels-1u;level>0u;level--) {
        push[4]=l->level_counts[level-1u];push[5]=l->level_words[level-1u];push[6]=l->level_words[level];
        compact_phase(gpu,2,push,(push[4]+DC_MPM_SCAN_LANES-1u)/DC_MPM_SCAN_LANES);
    }
    push[4]=l->root_word;push[5]=l->level_words[0];push[6]=0u;
    compact_phase(gpu,3,push,l->groups);
    compact_phase(gpu,4,push,0u);
}
void dc_gpu_record_mpm_activity(dc_gpu_t *gpu,bool rigid) {
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->mpm_activity_pipeline);
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    uint32_t push[7]={gpu->width,gpu->height,0u,rigid?1u:0u,0u,0u,0u};
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    dispatch_linear(gpu,gpu->mpm_scan_layout.tiles);
    activity_barrier(gpu,false);
    dc_gpu_record_active_compaction(gpu,rigid);
}
