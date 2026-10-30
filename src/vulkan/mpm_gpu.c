#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_internal.h"
#include "authored_rigid_gpu_internal.h"

enum {
    DC_MPM_SUBSTEPS = 2,
    DC_MPM_MODES = 6,
    DC_MPM_WATER_FEEDBACK_MODE = 6,
    DC_MPM_MOISTURE_MODE = 7
};

static uint32_t component_round_limit(uint32_t width, uint32_t height) {
    uint64_t cells = (uint64_t)width * height;
    uint32_t rounds = 1u;
    while (cells > 1u) {
        cells = (cells + 1u) / 2u;
        ++rounds;
    }
    return rounds;
}

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap)
        snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_mpm_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize particles = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
                             DC_MPM_PARTICLES_PER_CHUNK *
                             sizeof(dc_mpm_particle_t);
    VkDeviceSize flags = (VkDeviceSize)DC_GPU_CHUNK_SLOTS *
                         DC_MPM_PARTICLES_PER_CHUNK * sizeof(uint32_t);
    VkDeviceSize grid = (VkDeviceSize)gpu->width * gpu->height;
    VkDeviceSize component_bytes=dc_gpu_component_storage_bytes(gpu);
    if(!dc_gpu_mpm_scan_layout(gpu->width,gpu->height,&gpu->mpm_scan_layout))
        return error(err,cap,"Cannot size MPM activity scan scratch");
    return dc_gpu_make_device_buffer(gpu, particles, &gpu->mpm_proposal_buffer,
                                     &gpu->mpm_proposal_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, particles, &gpu->mpm_output_buffer,
                                     &gpu->mpm_output_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u, &gpu->mpm_grid_buffer,
                                     &gpu->mpm_grid_memory,
                                     &gpu->mpm_grid_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, grid * 16u, &gpu->mpm_force_buffer,
                                     &gpu->mpm_force_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 16u,
                                     &gpu->mpm_force_staging_buffer,
                                     &gpu->mpm_force_staging_memory,
                                     &gpu->mpm_force_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, grid * 8u, &gpu->mpm_velocity_buffer,
                                     &gpu->mpm_velocity_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, grid * 8u,
                                     &gpu->mpm_velocity_staging_buffer,
                                     &gpu->mpm_velocity_staging_memory,
                                     &gpu->mpm_velocity_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, flags, &gpu->mpm_accept_buffer,
                                     &gpu->mpm_accept_memory, err, cap) &&
           dc_gpu_make_device_buffer(
               gpu, (VkDeviceSize)gpu->mpm_scan_layout.words * sizeof(uint32_t),
               &gpu->mpm_activity_buffer, &gpu->mpm_activity_memory, err,
               cap) &&
           dc_gpu_make_mapped_buffer(gpu, component_bytes,
                                     &gpu->mpm_label_a_buffer,
                                     &gpu->mpm_label_a_memory,
                                     &gpu->mpm_label_a_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, component_bytes,
                                     &gpu->mpm_label_b_buffer,
                                     &gpu->mpm_label_b_memory,
                                     &gpu->mpm_label_b_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, component_bytes,
                                     &gpu->mpm_component_size_buffer,
                                     &gpu->mpm_component_size_memory,
                                     &gpu->mpm_component_size_mapped, err, cap);
}

static bool make_pipeline(dc_gpu_t *gpu, const char *path, uint32_t phase, VkPipeline *pipeline,
                          char *err, uint32_t cap) {
    bool mpm=strcmp(path,"build/shaders/mpm.comp.spv")==0;
    const char *early=getenv("DC_MPM_EARLY_LOADS");
    const char *commit=getenv("DC_MPM_COMMIT");
    if(phase==5u&&strcmp(path,"build/shaders/mpm.comp.spv")==0&&commit&&strcmp(commit,"packed")==0)
        path=gpu->runtime_body_consumers?"build/shaders/runtime/mpm_commit.comp.spv":"build/shaders/mpm_commit.comp.spv";
    const char *stencil=getenv("DC_MPM_GATHER_STENCIL");
    bool use_stencil=phase==4u&&(!stencil||strcmp(stencil,"1")==0);
    if(use_stencil) path=gpu->runtime_body_consumers?"build/shaders/runtime/mpm_stencil.comp.spv":
                                                     "build/shaders/mpm_stencil.comp.spv";
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &module, err, cap))
        return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main"},
        .layout = gpu->pipeline_layout};
    uint32_t values[]={phase,use_stencil?1u:0u,early&&strcmp(early,"1")==0,gpu->mpm_fused_grid};
    VkSpecializationMapEntry entries[]={{2u,0u,4u},{1u,4u,4u},{3u,8u,4u},{4u,12u,4u}};
    VkSpecializationInfo specialization={4u,entries,sizeof(values),values};
    if(phase!=UINT32_MAX||mpm) info.stage.pSpecializationInfo=&specialization;
    VkResult result = vkCreateComputePipelines(gpu->device, gpu->pipeline_cache, 1,
                                               &info, NULL, pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    if (result != VK_SUCCESS)
        return error(err, cap, "Cannot create MPM pipeline");
    return true;
}

static bool component_pipelines(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE, local = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(
            gpu, "build/shaders/component_extract.comp.spv", &module, err, cap))
        return false;
    if (!dc_gpu_load_shader_module(
            gpu, "build/shaders/component_local.comp.spv", &local, err, cap)) {
        vkDestroyShaderModule(gpu->device, module, NULL);
        return false;
    }
    VkSpecializationMapEntry entry = {.constantID = 0,
                                      .size = sizeof(uint32_t)};
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main"},
        .layout = gpu->pipeline_layout};
    VkResult result = VK_SUCCESS;
    for (uint32_t mode = 1; mode < (gpu->runtime_body_consumers?38u:35u) && result == VK_SUCCESS; ++mode) {
        if (mode == 33 && !gpu->runtime_body_consumers)
            continue;
        VkSpecializationInfo phase = {.mapEntryCount = 1,
                                      .pMapEntries = &entry,
                                      .dataSize = sizeof(mode),
                                      .pData = &mode};
        info.stage.pSpecializationInfo = &phase;
        info.stage.module = mode == 10 ? local : module;
        result = vkCreateComputePipelines(
            gpu->device, gpu->pipeline_cache, 1, &info, NULL,
            &gpu->component_extract_pipelines[mode]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    vkDestroyShaderModule(gpu->device, local, NULL);
    return result == VK_SUCCESS ||
           error(err, cap, "Cannot create extraction pipelines");
}
bool dc_gpu_component_objects_prepare(dc_gpu_t *gpu,char *err,uint32_t cap) {
    const uint32_t modes[]={7u,10u,12u,14u,23u,24u,38u};
    bool ready=true;
    for(unsigned i=0;i<sizeof(modes)/sizeof(*modes);i++) ready=ready&&gpu->component_object_pipelines[modes[i]]!=VK_NULL_HANDLE;
    if(ready) return true;
    VkShaderModule module=VK_NULL_HANDLE,local=VK_NULL_HANDLE;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/component_extract.comp.spv",&module,err,cap)) return false;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/component_local.comp.spv",&local,err,cap)) {
        vkDestroyShaderModule(gpu->device,module,NULL);return false;
    }
    VkSpecializationMapEntry entries[]={{0u,0u,4u},{1u,4u,4u}};
    VkResult result=VK_SUCCESS;
    for(unsigned i=0;i<sizeof(modes)/sizeof(*modes)&&result==VK_SUCCESS;i++) {
        uint32_t mode=modes[i];if(gpu->component_object_pipelines[mode]) continue;
        uint32_t values[]={mode,1u};
        VkSpecializationInfo specialization={.mapEntryCount=2,.pMapEntries=entries,.dataSize=sizeof(values),.pData=values};
        VkComputePipelineCreateInfo pipeline={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=mode==10u?local:module,.pName="main",.pSpecializationInfo=&specialization},.layout=gpu->pipeline_layout};
        result=vkCreateComputePipelines(gpu->device,gpu->pipeline_cache,1,&pipeline,NULL,&gpu->component_object_pipelines[mode]);
    }
    vkDestroyShaderModule(gpu->device,module,NULL);vkDestroyShaderModule(gpu->device,local,NULL);
    return result==VK_SUCCESS||error(err,cap,"Cannot prepare authored rigid-object pipelines");
}

bool dc_gpu_mpm_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    const char *fused=getenv("DC_MPM_FUSED_GRID");
    gpu->mpm_fused_grid=!fused||strcmp(fused,"1")==0;
    if(!make_pipeline(gpu,"build/shaders/mpm.comp.spv",UINT32_MAX,&gpu->mpm_pipeline,err,cap)||
       !dc_gpu_mpm_swap_init(gpu,err,cap)) return false;
    const char *variant=getenv("DC_MPM_KERNELS");
    bool specialized=!variant||strcmp(variant,"specialized")==0;
    if(variant&&!specialized&&strcmp(variant,"generic")!=0) return error(err,cap,"Invalid MPM kernel variant");
    for(uint32_t phase=0;specialized&&phase<DC_GPU_MPM_PHASE_COUNT;phase++) {
        if(phase==5u&&gpu->mpm_swap_buffers) continue;
        if(!make_pipeline(gpu,"build/shaders/mpm.comp.spv",phase,&gpu->mpm_phase_pipelines[phase],err,cap)) return false;
    }
    const char *commit=getenv("DC_MPM_COMMIT");
    if(!specialized&&!gpu->mpm_swap_buffers&&commit&&strcmp(commit,"packed")==0&&
       !make_pipeline(gpu,"build/shaders/mpm.comp.spv",5u,&gpu->mpm_phase_pipelines[5],err,cap)) return false;
    return make_pipeline(gpu,"build/shaders/mpm_active.comp.spv",UINT32_MAX,
                         &gpu->mpm_activity_pipeline,err,cap) &&
           dc_gpu_mpm_compaction_pipeline_init(gpu,err,cap) &&
           make_pipeline(gpu,"build/shaders/mpm_component.comp.spv",UINT32_MAX,
                         &gpu->mpm_component_pipeline,err,cap) &&
           component_pipelines(gpu,err,cap);
}

void dc_gpu_mpm_pipeline_destroy(dc_gpu_t *gpu) {
    for(unsigned mode=0;mode<39;mode++) if(gpu->component_object_pipelines[mode])
        vkDestroyPipeline(gpu->device,gpu->component_object_pipelines[mode],NULL);
    if(gpu->mpm_swap_commit_pipeline) vkDestroyPipeline(gpu->device,gpu->mpm_swap_commit_pipeline,NULL);
    dc_gpu_mpm_compaction_pipeline_destroy(gpu);
    for (unsigned mode = 0; mode < 38; ++mode)
        if (gpu->component_extract_pipelines[mode])
            vkDestroyPipeline(gpu->device,
                              gpu->component_extract_pipelines[mode], NULL);
    for(unsigned phase=0;phase<DC_GPU_MPM_PHASE_COUNT;phase++)
        if(gpu->mpm_phase_pipelines[phase]) vkDestroyPipeline(gpu->device,gpu->mpm_phase_pipelines[phase],NULL);
    if (gpu->mpm_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_pipeline, NULL);
    if (gpu->mpm_activity_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_activity_pipeline, NULL);
    if (gpu->mpm_component_pipeline)
        vkDestroyPipeline(gpu->device, gpu->mpm_component_pipeline, NULL);
}

#define DESTROY_MPM_BUFFER(name)                                               \
    do {                                                                       \
        if (gpu->mpm_##name##_buffer)                                          \
            vkDestroyBuffer(gpu->device, gpu->mpm_##name##_buffer, NULL);      \
        if (gpu->mpm_##name##_memory)                                          \
            vkFreeMemory(gpu->device, gpu->mpm_##name##_memory, NULL);         \
    } while (0)

void dc_gpu_mpm_buffers_destroy(dc_gpu_t *gpu) {
    if (gpu->mpm_grid_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_grid_memory);
    if (gpu->mpm_label_a_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_label_a_memory);
    if (gpu->mpm_label_b_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_label_b_memory);
    if (gpu->mpm_component_size_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_component_size_memory);
    DESTROY_MPM_BUFFER(proposal);
    DESTROY_MPM_BUFFER(output);
    DESTROY_MPM_BUFFER(grid);
    if (gpu->mpm_force_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_force_staging_memory);
    if (gpu->mpm_velocity_mapped)
        vkUnmapMemory(gpu->device, gpu->mpm_velocity_staging_memory);
    if (gpu->mpm_force_staging_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_force_staging_buffer, NULL);
    if (gpu->mpm_velocity_staging_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_velocity_staging_buffer, NULL);
    if (gpu->mpm_force_staging_memory)
        vkFreeMemory(gpu->device, gpu->mpm_force_staging_memory, NULL);
    if (gpu->mpm_velocity_staging_memory)
        vkFreeMemory(gpu->device, gpu->mpm_velocity_staging_memory, NULL);
    if (gpu->mpm_force_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_force_buffer, NULL);
    if (gpu->mpm_velocity_buffer)
        vkDestroyBuffer(gpu->device, gpu->mpm_velocity_buffer, NULL);
    if (gpu->mpm_force_memory)
        vkFreeMemory(gpu->device, gpu->mpm_force_memory, NULL);
    if (gpu->mpm_velocity_memory)
        vkFreeMemory(gpu->device, gpu->mpm_velocity_memory, NULL);
    DESTROY_MPM_BUFFER(accept);
    DESTROY_MPM_BUFFER(activity);
    DESTROY_MPM_BUFFER(label_a);
    DESTROY_MPM_BUFFER(label_b);
    DESTROY_MPM_BUFFER(component_size);
}
#undef DESTROY_MPM_BUFFER

bool dc_gpu_mpm_readback_scratch(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu)
        return error(err, cap, "Invalid MPM diagnostic readback");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset MPM diagnostic command");
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin MPM diagnostic command");
    VkMemoryBarrier2 before = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &before};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    VkBufferCopy force_copy = {.size = (VkDeviceSize)gpu->width * gpu->height *
                                       16u};
    VkBufferCopy velocity_copy = {.size = (VkDeviceSize)gpu->width *
                                          gpu->height * 8u};
    vkCmdCopyBuffer(gpu->command, gpu->mpm_force_buffer,
                    gpu->mpm_force_staging_buffer, 1, &force_copy);
    vkCmdCopyBuffer(gpu->command, gpu->mpm_velocity_buffer,
                    gpu->mpm_velocity_staging_buffer, 1, &velocity_copy);
    VkMemoryBarrier2 after = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                              .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                              .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                              .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT};
    dependency.pMemoryBarriers = &after;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end MPM diagnostic command");
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1,
                           .pCommandBuffers = &gpu->command};
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "MPM diagnostic readback failed");
    return true;
}

static void mpm_barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 target_stage,
                        VkAccessFlags2 target_access) {
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = target_stage,
        .dstAccessMask = target_access};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_components(dc_gpu_t *gpu, bool rigid) {
    dc_gpu_record_mpm_activity(gpu,rigid);
    uint32_t push[7] = {gpu->width, gpu->height, 0u, rigid ? 1u : 0u,
                        0u,         0u,          0u};
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->mpm_component_pipeline);
    VkDeviceSize component_indirect =
        (VkDeviceSize)(4u + 2u * ((gpu->width + 15u) / 16u) *
                                ((gpu->height + 15u) / 16u)) *
        sizeof(uint32_t);
    uint32_t rounds = component_round_limit(gpu->width, gpu->height);
    for (uint32_t round = 0; round < rounds + 2u; ++round) {
        uint32_t first = round == 0u ? 0u : round == rounds + 1u ? 4u : 1u;
        uint32_t last = round == 0u ? 0u : round == rounds + 1u ? 4u : 3u;
        for (uint32_t mode = first; mode <= last; ++mode) {
            push[2] = mode;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               push);
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                                  mode >= 1u && mode <= 3u ? component_indirect
                                                           : 0);
            mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }
        if (round > 0u && round <= rounds) {
            push[2] = 5u;
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               push);
            vkCmdDispatch(gpu->command, 1u, 1u, 1u);
            mpm_barrier(gpu,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        }
    }
}

static void bind_mpm_phase(dc_gpu_t *gpu,uint32_t phase,VkPipeline *bound) {
    VkPipeline pipeline=gpu->mpm_phase_pipelines[phase];
    if(!pipeline) pipeline=gpu->mpm_pipeline;
    if(pipeline==*bound) return;
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    *bound=pipeline;
}

void dc_gpu_record_mpm_uncached(dc_gpu_t *gpu) {
    dc_gpu_mpm_swap_descriptors(gpu);
    dc_gpu_record_components(gpu, false);
    uint32_t push[7] = {gpu->width, gpu->height, 0u, 0u, 0u, 0u, 0u};
    VkPipeline bound=VK_NULL_HANDLE;
    bind_mpm_phase(gpu,DC_MPM_MOISTURE_MODE,&bound);
    push[2] = DC_MPM_MOISTURE_MODE;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
    mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    for (uint32_t step = 0; step < DC_MPM_SUBSTEPS; ++step) {
        push[3]=gpu->mpm_swap_buffers?(step&1u):0u;
        for (uint32_t mode = 0; mode < DC_MPM_MODES; ++mode) {
            if(gpu->mpm_fused_grid&&mode==1u) continue;
            push[2] = mode;
            if(gpu->mpm_swap_buffers&&mode==5u) {
                vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->mpm_swap_commit_pipeline);
                bound=gpu->mpm_swap_commit_pipeline;
            } else bind_mpm_phase(gpu,mode,&bound);
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               push);
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer, 0);
            mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            if(gpu->mpm_swap_buffers&&mode==5u) {
                VkDescriptorSet next=(step&1u)?gpu->descriptor:gpu->mpm_swapped_descriptor;
                vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
                    gpu->pipeline_layout,0,1,&next,0,NULL);
            }
            if (mode == 1u) {
                push[2] = DC_MPM_WATER_FEEDBACK_MODE;
                bind_mpm_phase(gpu,DC_MPM_WATER_FEEDBACK_MODE,&bound);
                vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                                   push);
                vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                                      0);
                mpm_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            }
        }
    }
}
