#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "contact_shader_contract.h"
#include "gpu_internal.h"

_Static_assert(sizeof(dc_gpu_contact_t) == 128, "GPU contact layout must match SPIR-V");
_Static_assert(offsetof(dc_gpu_contact_t, anchor_a) == 64, "GPU contact anchor offset changed");
_Static_assert(sizeof(dc_gpu_contact_stats_t) == 24, "GPU contact counters must match SPIR-V");
_Static_assert(sizeof(dc_gpu_contact_anchor_t) == 24, "GPU world anchor layout must match SPIR-V");
_Static_assert(offsetof(dc_gpu_contact_t, feature_chunk) == 112, "GPU contact feature offset changed");
_Static_assert(DC_CONTACT_FINISH_ACTIVITY_WORD + 3u <= DC_MPM_ACTIVITY_MIN_WORDS,
               "Contact planning must fit the minimum activity allocation");

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_set_contact_capacity(dc_gpu_t *gpu, uint32_t capacity, char *err, uint32_t cap) {
    if (!gpu || capacity > (gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY))
        return error(err, cap, "Invalid contact capacity");
    dc_gpu_contact_data(gpu)[DC_GPU_CONTACT_CAPACITY_WORD] = capacity;
    gpu->body_refresh_pending = true;
    return true;
}

bool dc_gpu_read_contacts(dc_gpu_t *gpu, dc_gpu_contact_stats_t *stats,
                          dc_gpu_contact_t *contacts, uint32_t contact_cap,
                          char *err, uint32_t cap) {
    if (!gpu || !stats || (!contacts && contact_cap))
        return error(err, cap, "Invalid contact readback");
    const uint32_t *data = dc_gpu_contact_data(gpu);
    dc_gpu_contact_stats_t completed;
    memcpy(&completed, data, sizeof(completed));
    if (completed.count > (gpu->runtime_contact_capacity?gpu->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY) || (contacts && contact_cap < completed.count))
        return error(err, cap, "Contact readback buffer is too small");
    if (contacts) memcpy(contacts, data + DC_GPU_CONTACT_HEADER_WORDS,
                         (size_t)completed.count * sizeof(*contacts));
    *stats = completed;
    return true;
}

bool dc_gpu_contact_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if(!dc_gpu_contact_shader_compatible("build/shaders/contacts.comp.spv",
        DC_GPU_TYPED_CONTACT_ACTIVITY_WORD,err,cap)) return false;
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/contacts.comp.spv", &module, err, cap)) return false;
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = VK_SUCCESS;
    VkSpecializationMapEntry entry = {.constantID=0, .offset=0, .size=sizeof(uint32_t)};
    for (uint32_t phase=0; phase<DC_GPU_CONTACT_PHASE_COUNT && result==VK_SUCCESS; ++phase) {
        VkSpecializationInfo specialization = {.mapEntryCount=1, .pMapEntries=&entry,
            .dataSize=sizeof(phase), .pData=&phase};
        info.stage.pSpecializationInfo=&specialization;
        result = vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,&gpu->contact_pipelines[phase]);
    }
    vkDestroyShaderModule(gpu->device,module,NULL);
    if(result!=VK_SUCCESS) return error(err,cap,"Cannot create generic contact pipelines");
    if(!dc_gpu_contact_shader_pixels("build/shaders/contacts_pixel.comp.spv",
        DC_GPU_TYPED_CONTACT_ACTIVITY_WORD,err,cap)) return false;
    if(!dc_gpu_load_shader_module(gpu,"build/shaders/contacts_pixel.comp.spv",&module,err,cap)) return false;
    info.stage.module=module;
    const uint32_t pixel_phases[]={4u,5u,2u,3u,6u,7u};
    VkPipeline *pixel_pipelines[]={&gpu->contact_pixel_plan_pipeline,
        &gpu->contact_pixel_work_pipeline,&gpu->contact_pixel_pipeline,&gpu->contact_pixel_finish_pipeline,
        &gpu->contact_terrain_pipeline,&gpu->contact_boundary_pipeline};
    for(unsigned i=0;i<6u&&result==VK_SUCCESS;i++) {
        uint32_t values[]={pixel_phases[i],1u};
        VkSpecializationMapEntry entries[]={{.constantID=0,.size=sizeof(uint32_t)},
            {.constantID=1,.offset=sizeof(uint32_t),.size=sizeof(uint32_t)}};
        VkSpecializationInfo specialization={.mapEntryCount=2,.pMapEntries=entries,
            .dataSize=sizeof(values),.pData=values};
        info.stage.pSpecializationInfo=&specialization;
        result=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,pixel_pipelines[i]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS ? true : error(err, cap, "Cannot create GPU contact pipeline");
}

void dc_gpu_record_contact_terrain(dc_gpu_t *gpu) {
    if(!gpu->runtime_body_consumers||!gpu->contact_terrain_pipeline) return;
    uint32_t push[8]={gpu->width,gpu->height,6u,0,0,0,0,0};
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->contact_terrain_pipeline);
    vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(gpu->command,(gpu->width+63u)/64u,(gpu->height+63u)/64u,1u);
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(gpu->command,&dependency);
    if(gpu->contact_boundary_pipeline) {
        uint32_t count=dc_gpu_runtime_body_count(gpu),groups=count?count:1u;
        uint32_t columns=groups<65535u?groups:65535u;
        push[2]=7u;push[3]=count;
        push[4]=dc_gpu_pixel_pool_word_offset(gpu)+DC_GPU_PIXEL_POOL_HEADER_WORDS;
        vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->contact_boundary_pipeline);
        vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(gpu->command,columns,DC_CONTACT_PIXEL_GROUPS,(groups-1u)/columns+1u);
        vkCmdPipelineBarrier2(gpu->command,&dependency);
    }
}
void dc_gpu_record_contacts(dc_gpu_t *gpu) {
    if(!gpu->solver_recording) dc_gpu_record_contact_terrain(gpu);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width, gpu->height, 0, dc_gpu_runtime_body_count(gpu),
        (uint32_t)x, (uint32_t)(x >> 32), (uint32_t)y, (uint32_t)(y >> 32)};
    uint32_t count=DC_GPU_CONTACT_PHASE_COUNT+4u*(gpu->contact_pixel_pipeline!=VK_NULL_HANDLE);
    for (uint32_t pass = 0; pass < count; ++pass) {
        bool typed=pass>=DC_GPU_CONTACT_PHASE_COUNT;
        uint32_t mode=typed?(pass==3u?4u:pass==4u?5u:pass==5u?2u:3u):pass;
        VkPipeline pipeline=typed?(pass==3u?gpu->contact_pixel_plan_pipeline:
            pass==4u?gpu->contact_pixel_work_pipeline:
            pass==5u?gpu->contact_pixel_pipeline:gpu->contact_pixel_finish_pipeline):
            gpu->contact_pipelines[mode];
        vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline);
        push[2] = mode | (gpu->component_profile ? 256u : 0u) |
            (gpu->contact_pixel_pipeline==VK_NULL_HANDLE?512u:1024u) |
            (gpu->runtime_body_consumers&&gpu->contact_terrain_pipeline?2048u:0u) |
            (gpu->runtime_body_consumers&&gpu->contact_boundary_pipeline?4096u:0u);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        if(typed&&pass==4u) vkCmdDispatch(gpu->command,1,1,1);
        else if(typed) {
            uint32_t word=pass==3u?DC_CONTACT_PLAN_ACTIVITY_WORD:
                pass==5u?DC_GPU_TYPED_CONTACT_ACTIVITY_WORD:DC_CONTACT_FINISH_ACTIVITY_WORD;
            vkCmdDispatchIndirect(gpu->command,gpu->mpm_activity_buffer,word*sizeof(uint32_t));
        }
        else if (mode) {
            uint32_t word = mode == 2 ? dc_gpu_solver_word_offset(gpu) + DC_GPU_PIXEL_CONTACT_DISPATCH_WORD :
                dc_gpu_contact_word_offset(gpu) + DC_GPU_CONTACT_DISPATCH_WORD;
            VkDeviceSize indirect = sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
                (VkDeviceSize)word * sizeof(uint32_t);
            vkCmdDispatchIndirect(gpu->command, gpu->body_buffer, indirect);
        } else vkCmdDispatch(gpu->command, 1, 1, 1);
        bool initial=pass==0u,final=pass+1u==count;
        if(!initial&&!final&&!typed) continue;
        VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                            ((initial||typed)?VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT:0) |
                            (final&&!gpu->solver_recording?VK_PIPELINE_STAGE_2_HOST_BIT:0),
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                             ((initial||typed)?VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT:0) |
                             (final&&!gpu->solver_recording?VK_ACCESS_2_HOST_READ_BIT:0) };
        VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
        vkCmdPipelineBarrier2(gpu->command, &dependency);
    }
}
