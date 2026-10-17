#include <stdio.h>

#include "gpu_internal.h"

bool dc_gpu_draw_pipeline_init(dc_gpu_t *gpu,VkShaderModule module,char *err,uint32_t cap) {
    VkPipeline *outputs[]={&gpu->simple_draw_pipeline,&gpu->pixel_draw_pipeline};
    for(unsigned i=0;i<2;i++) {
        uint32_t values[]={i==1u,i==0u};
        VkSpecializationMapEntry entries[]={{.constantID=1,.size=4},
            {.constantID=2,.offset=4,.size=4}};
        VkSpecializationInfo constants={.mapEntryCount=2,.pMapEntries=entries,
            .dataSize=sizeof(values),.pData=values};
        VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main",
                .pSpecializationInfo=&constants},.layout=gpu->pipeline_layout};
        VkResult result=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,outputs[i]);
        if(result!=VK_SUCCESS) {
            if(err&&cap) snprintf(err,cap,"Cannot create draw variant%u: %d",i,result);
            return false;
        }
    }
    return true;
}

static void draw_barrier(dc_gpu_t *gpu,bool indirect) {
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|
            (indirect?VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT:0),
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|
            (indirect?VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT:0)};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(gpu->command,&dependency);
}

static void draw_pipeline(dc_gpu_t *gpu,VkPipeline pipeline,const uint32_t push[7],int offset) {
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,
        0,7*sizeof(uint32_t),push);
    if(offset>=0) vkCmdDispatchIndirect(gpu->command,gpu->mpm_activity_buffer,(VkDeviceSize)offset);
    else vkCmdDispatch(gpu->command,(gpu->view_width+15u)/16u,(gpu->view_height+15u)/16u,1);
}

void dc_gpu_record_draw(dc_gpu_t *gpu,const uint32_t push[7]) {
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    if(push[4]!=DC_GPU_OVERLAY_STRUCTURAL_STRESS||!gpu->pixel_draw_pipeline) {
        VkPipeline pipeline=push[4]!=DC_GPU_OVERLAY_STRUCTURAL_STRESS&&gpu->simple_draw_pipeline?
            gpu->simple_draw_pipeline:gpu->pipeline;
        draw_pipeline(gpu,pipeline,push,-1);
        return;
    }
    uint32_t classify[]={gpu->width,gpu->height,512u,dc_gpu_runtime_body_count(gpu),
        gpu->view_width,gpu->view_height,0,0};
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->rigid_classify_pipeline);
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,
        0,sizeof(classify),classify);
    vkCmdDispatch(gpu->command,1,1,1);
    draw_barrier(gpu,true);
    draw_pipeline(gpu,gpu->pipeline,push,0);
    draw_barrier(gpu,false);
    draw_pipeline(gpu,gpu->pixel_draw_pipeline,push,3*sizeof(uint32_t));
}
