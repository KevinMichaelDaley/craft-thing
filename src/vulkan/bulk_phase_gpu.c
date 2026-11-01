#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "gas_gpu_internal.h"
#include "gpu_internal.h"

#define FR_BULK_GRAVITY_CELLS_PER_SECOND_SQUARED 900.f

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static bool ensure_pipeline(dc_gpu_t *gpu,char *err,uint32_t cap);
bool fr_bulk_phase_gpu_import_chunk(dc_gpu_t *gpu,uint32_t slot,
    const dc_chunk_t *chunk,const fr_bulk_phase_chunk_t *phase,char *err,uint32_t cap) {
    if(!gpu||!gpu->bulk_phase_mapped||slot>=DC_GPU_CHUNK_SLOTS||!chunk||!phase)
        return error(err,cap,"Invalid bulk phase chunk import");
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if((uint64_t)chunk->cells[i].fluid_mass+phase->oil_fp[i]+phase->slime_fp[i]>
            FR_BULK_CELL_FULL_FP||phase->oil_retained_fp[i]>phase->oil_fp[i]||
            phase->slime_retained_fp[i]>phase->slime_fp[i])
            return error(err,cap,"Bulk phase inventory or retained volume is invalid");
    if(vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS)
        return error(err,cap,"Cannot complete bulk phase chunk import");
    bool occupied=false;
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if(phase->oil_fp[i]||phase->slime_fp[i]) {occupied=true;break;}
    if(occupied&&!ensure_pipeline(gpu,err,cap)) return false;
    memcpy((fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped+slot,phase,sizeof(*phase));
    gpu->bulk_phase_work_possible|=occupied;
    fr_gas_gpu_geometry_changed(gpu,false);
    return true;
}
bool fr_bulk_phase_gpu_export_chunk(dc_gpu_t *gpu,uint32_t slot,
    fr_bulk_phase_chunk_t *out,char *err,uint32_t cap) {
    if(!gpu||!gpu->bulk_phase_mapped||slot>=DC_GPU_CHUNK_SLOTS||!out)
        return error(err,cap,"Invalid bulk phase chunk export");
    if(vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS)
        return error(err,cap,"Cannot complete bulk phase chunk export");
    memcpy(out,(const fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped+slot,sizeof(*out));
    return true;
}
static bool ensure_pipeline(dc_gpu_t *gpu,char *err,uint32_t cap) {
    if(gpu->bulk_phase_pipeline&&gpu->bulk_phase_retained_pipeline) return true;
    VkShaderModule module=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/bulk_phase_pair.comp.spv",&module,err,cap))
        return false;
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"},
        .layout=gpu->pipeline_layout};
    VkResult result=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,
        &gpu->bulk_phase_pipeline);
    vkDestroyShaderModule(gpu->device,module,NULL);
    if(result!=VK_SUCCESS) return error(err,cap,"Cannot create bulk phase pair pipeline");
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/bulk_phase_retained.comp.spv",&module,err,cap))
        return false;
    info.stage.module=module;
    result=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,
        &gpu->bulk_phase_retained_pipeline);
    vkDestroyShaderModule(gpu->device,module,NULL);
    return result==VK_SUCCESS||error(err,cap,"Cannot create bulk phase retained pipeline");
}
void dc_gpu_record_bulk_phase(dc_gpu_t *gpu,float seconds) {
    if(!gpu->bulk_phase_work_possible||!gpu->bulk_phase_pipeline||seconds<=0.f) return;
    const fr_reactive_profile_t *water=fr_reactive_profile(DC_MATERIAL_WATER);
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *slime=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    VkMemoryBarrier2 barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount=1,.pMemoryBarriers=&barrier};
    vkCmdPipelineBarrier2(gpu->command,&dependency);
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->bulk_phase_pipeline);
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    struct {uint32_t width,height,parity,axis;float seconds,gravity,water,oil,slime;} push={
        gpu->width,gpu->height,0,0,seconds,FR_BULK_GRAVITY_CELLS_PER_SECOND_SQUARED,
        water->density,oil->density,slime->density};
    for(uint32_t axis=0;axis<2u;axis++) for(uint32_t parity=0;parity<2u;parity++) {
        push.parity=parity;push.axis=axis;
        vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,
            0,sizeof(push),&push);
        vkCmdDispatch(gpu->command,(gpu->width*gpu->height+63u)/64u,1,1);
        vkCmdPipelineBarrier2(gpu->command,&dependency);
    }
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->bulk_phase_retained_pipeline);
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,
        0,2u*sizeof(uint32_t),&push);
    vkCmdDispatch(gpu->command,(gpu->width*gpu->height+63u)/64u,1,1);
    gpu->fluid_snapshot_valid=false;gpu->fluid_work_possible=true;
    fr_gas_gpu_geometry_changed(gpu,false);
}
bool fr_bulk_phase_gpu_step(dc_gpu_t *gpu,float seconds,char *err,uint32_t cap) {
    if(!gpu||!isfinite(seconds)||seconds<0.f)
        return error(err,cap,"Invalid bulk phase interval");
    if(seconds==0.f) return true;
    const fr_reactive_profile_t *water=fr_reactive_profile(DC_MATERIAL_WATER);
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *slime=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    if(!water||!oil||!slime||!ensure_pipeline(gpu,err,cap))
        return error(err,cap,"Cannot configure bulk phase density profiles");
    if(vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS||
       vkResetCommandBuffer(gpu->command,0)!=VK_SUCCESS)
        return error(err,cap,"Cannot reset bulk phase command buffer");
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkBeginCommandBuffer(gpu->command,&begin)!=VK_SUCCESS)
        return error(err,cap,"Cannot begin bulk phase command buffer");
    VkMemoryBarrier2 barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_HOST_BIT,.srcAccessMask=VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount=1,.pMemoryBarriers=&barrier};
    vkCmdPipelineBarrier2(gpu->command,&dependency);
    gpu->bulk_phase_work_possible=true;
    dc_gpu_record_bulk_phase(gpu,seconds);
    barrier.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    barrier.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(gpu->command,&dependency);
    if(vkEndCommandBuffer(gpu->command)!=VK_SUCCESS)
        return error(err,cap,"Cannot end bulk phase command buffer");
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount=1,.pCommandBuffers=&gpu->command};
    if(vkQueueSubmit(gpu->queue,1,&submit,VK_NULL_HANDLE)!=VK_SUCCESS||
       vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS)
        return error(err,cap,"Bulk phase pair submission failed");
    gpu->fluid_snapshot_valid=false;gpu->fluid_work_possible=true;
    fr_gas_gpu_geometry_changed(gpu,false);
    return true;
}
