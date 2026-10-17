#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_internal.h"

bool dc_gpu_fluid_displacement_overflow(dc_gpu_t *gpu, uint64_t *mass,
                                      char *err, uint32_t cap) {
    if (!gpu || !mass) return false;
    if (!dc_gpu_copy_chunk_state(gpu, UINT32_MAX, false, false, err, cap)) return false;
    const uint32_t *pages = gpu->page_mapped;
    const uint32_t *occupancy = gpu->occupancy_mapped;
    const dc_cell_t *cells = gpu->chunk_mapped;
    uint64_t total = 0;
    for (uint32_t y = 0; y < gpu->height; ++y)
        for (uint32_t x = 0; x < gpu->width; ++x) {
            uint32_t page = pages[(y / 64u) * gpu->page_width + x / 64u];
            if (page && occupancy[y * gpu->width + x]) {
                uint32_t water=cells[(page - 1u) * DC_CHUNK_CELLS +
                    (y % 64u) * 64u + x % 64u].fluid_mass;
                uint32_t covered=occupancy[2u*gpu->width*gpu->height+y*gpu->width+x];
                uint32_t capacity=DC_FLUID_FULL-(covered<DC_FLUID_FULL?covered:DC_FLUID_FULL);
                if(water>capacity) total+=water-capacity;
            }
        }
    *mass = total;
    return true;
}

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

#ifndef DC_PRESSURE_SWEEPS
#define DC_PRESSURE_SWEEPS 20u
#endif

enum { FLUID_DISPLACEMENT_MODE = 11u, FLUID_LATERAL_DISPLACEMENT_MODE = 12u };

bool dc_gpu_fluid_buffers_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize bytes = (VkDeviceSize)gpu->width * gpu->height * sizeof(uint32_t);
    return dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_a_buffer,
               &gpu->fluid_a_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_b_buffer,
               &gpu->fluid_b_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, bytes * 2, &gpu->velocity_buffer,
               &gpu->velocity_memory, &gpu->velocity_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->pressure_a_buffer,
               &gpu->pressure_a_memory, err, cap) &&
           dc_gpu_make_device_buffer(gpu, bytes, &gpu->fluid_previous_buffer,
               &gpu->fluid_previous_memory, err, cap);
}

static bool make_pipeline(dc_gpu_t *gpu, const char *path, uint32_t phase, VkPipeline *pipeline,
                          char *err, uint32_t cap) {
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &shader, err, cap)) return false;
    bool transport=strcmp(path,"build/shaders/fluid_transport.comp.spv")==0;
    bool displacement=strcmp(path,"build/shaders/fluid_displacement.comp.spv")==0;
    uint32_t length=(displacement?phase==0:phase<2)?gpu->height:gpu->width;
    uint32_t values[2]={phase,length<2048u?length:2048u};
    VkSpecializationMapEntry entries[2]={{.constantID=0,.size=sizeof(uint32_t)},
        {.constantID=1,.offset=sizeof(uint32_t),.size=sizeof(uint32_t)}};
    VkSpecializationInfo specialization = {.mapEntryCount=transport||displacement?2u:1u,.pMapEntries=entries,
        .dataSize=transport||displacement?sizeof(values):sizeof(phase),.pData=values};
    VkComputePipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main",
            .pSpecializationInfo=phase==UINT32_MAX?NULL:&specialization },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                                1, &info, NULL, pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create Eulerian fluid pipeline");
    return true;
}

bool dc_gpu_fluid_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    const char *variant=getenv("DC_FLUID_VARIANTS");
    bool specialized=variant&&strcmp(variant,"specialized")==0;
    if(variant&&!specialized&&strcmp(variant,"generic")!=0)
        return error(err,cap,"DC_FLUID_VARIANTS must be generic or specialized");
    const char *fluid=variant?"build/diagnostics/fluid-variants/fluid.spv":"build/shaders/fluid.comp.spv";
    const char *projection=variant?"build/diagnostics/fluid-variants/projection.spv":"build/shaders/projection.comp.spv";
    if(specialized) {
        for(uint32_t phase=0;phase<13;phase++)
            if(!make_pipeline(gpu,fluid,phase,&gpu->fluid_phase_pipelines[phase],err,cap)) return false;
        for(uint32_t phase=0;phase<6;phase++) {
            if(phase==4) continue;
            if(!make_pipeline(gpu,projection,phase,&gpu->projection_phase_pipelines[phase],err,cap)) return false;
        }
    } else if(!make_pipeline(gpu,fluid,UINT32_MAX,&gpu->fluid_pipeline,err,cap)||
              !make_pipeline(gpu,projection,UINT32_MAX,&gpu->projection_pipeline,err,cap)) return false;
    if (!(make_pipeline(gpu, "build/shaders/hydro_head.comp.spv", UINT32_MAX,
                         &gpu->hydro_head_pipeline, err, cap) &&
           make_pipeline(gpu, "build/shaders/fluid_aperture.comp.spv", UINT32_MAX,
                         &gpu->fluid_aperture_pipeline, err, cap) &&
           make_pipeline(gpu, "build/shaders/shift_velocity.comp.spv", UINT32_MAX,
                         &gpu->velocity_shift_pipeline, err, cap))) return false;
    for (uint32_t mode = 0; mode < 4; mode++)
        if (!make_pipeline(gpu, "build/shaders/fluid_transport.comp.spv", mode,
                            &gpu->fluid_transport_pipelines[mode], err, cap)) return false;
    for (uint32_t phase = 0; phase < 2; phase++)
        if (!make_pipeline(gpu, "build/shaders/fluid_displacement.comp.spv", phase,
                            &gpu->fluid_displacement_pipelines[phase], err, cap)) return false;
    return true;
}

static void bind_fluid_mode(dc_gpu_t *gpu,uint32_t mode,bool projection) {
    VkPipeline pipeline=projection?gpu->projection_phase_pipelines[mode]:gpu->fluid_phase_pipelines[mode];
    if(!pipeline) pipeline=projection?gpu->projection_pipeline:gpu->fluid_pipeline;
    if(!projection&&mode<4&&gpu->rigid_occupancy_present) pipeline=gpu->fluid_transport_pipelines[mode];
    if(!projection&&mode==FLUID_DISPLACEMENT_MODE) pipeline=gpu->fluid_displacement_pipelines[0];
    if(!projection&&mode==FLUID_LATERAL_DISPLACEMENT_MODE) pipeline=gpu->fluid_displacement_pipelines[1];
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
}

void dc_gpu_fluid_destroy(dc_gpu_t *gpu) {
    for(uint32_t i=0;i<2;i++) if(gpu->fluid_displacement_pipelines[i]) vkDestroyPipeline(gpu->device,gpu->fluid_displacement_pipelines[i],NULL);
    for(uint32_t i=0;i<4;i++) if(gpu->fluid_transport_pipelines[i]) vkDestroyPipeline(gpu->device,gpu->fluid_transport_pipelines[i],NULL);
    if (gpu->fluid_aperture_pipeline) vkDestroyPipeline(gpu->device, gpu->fluid_aperture_pipeline, NULL);
    for(uint32_t i=0;i<13;i++) if(gpu->fluid_phase_pipelines[i]) vkDestroyPipeline(gpu->device,gpu->fluid_phase_pipelines[i],NULL);
    for(uint32_t i=0;i<6;i++) if(gpu->projection_phase_pipelines[i]) vkDestroyPipeline(gpu->device,gpu->projection_phase_pipelines[i],NULL);
    if (gpu->fluid_pipeline) vkDestroyPipeline(gpu->device, gpu->fluid_pipeline, NULL);
    if (gpu->projection_pipeline) vkDestroyPipeline(gpu->device, gpu->projection_pipeline, NULL);
    if (gpu->hydro_head_pipeline) vkDestroyPipeline(gpu->device, gpu->hydro_head_pipeline, NULL);
    if (gpu->velocity_shift_pipeline)
        vkDestroyPipeline(gpu->device, gpu->velocity_shift_pipeline, NULL);
    if (gpu->velocity_mapped) vkUnmapMemory(gpu->device, gpu->velocity_memory);
    if (gpu->fluid_a_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_a_buffer, NULL);
    if (gpu->fluid_b_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_b_buffer, NULL);
    if (gpu->velocity_buffer) vkDestroyBuffer(gpu->device, gpu->velocity_buffer, NULL);
    if (gpu->pressure_a_buffer) vkDestroyBuffer(gpu->device, gpu->pressure_a_buffer, NULL);
    if (gpu->fluid_previous_buffer) vkDestroyBuffer(gpu->device, gpu->fluid_previous_buffer, NULL);
    if (gpu->fluid_a_memory) vkFreeMemory(gpu->device, gpu->fluid_a_memory, NULL);
    if (gpu->fluid_b_memory) vkFreeMemory(gpu->device, gpu->fluid_b_memory, NULL);
    if (gpu->velocity_memory) vkFreeMemory(gpu->device, gpu->velocity_memory, NULL);
    if (gpu->pressure_a_memory) vkFreeMemory(gpu->device, gpu->pressure_a_memory, NULL);
    if (gpu->fluid_previous_memory) vkFreeMemory(gpu->device, gpu->fluid_previous_memory, NULL);
}

static void fluid_barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 source_stage,
                          VkAccessFlags2 source_access, VkPipelineStageFlags2 target_stage,
                          VkAccessFlags2 target_access) {
    VkMemoryBarrier2 memory = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = source_stage, .srcAccessMask = source_access,
        .dstStageMask = target_stage, .dstAccessMask = target_access };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &memory };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

bool dc_gpu_shift_velocity(dc_gpu_t *gpu, int32_t chunk_dx, int32_t chunk_dy,
                           char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "Invalid GPU velocity shift");
    if (!chunk_dx && !chunk_dy) {
        return true;
    }
    for (uint32_t slot = 0; slot < DC_GPU_CHUNK_SLOTS; ++slot)
        gpu->preserve_shifted_slot[slot] = gpu->slot_page[slot] != UINT32_MAX;
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset velocity shift command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin velocity shift command buffer");
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->velocity_shift_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 0u,
                         (uint32_t)chunk_dx, (uint32_t)chunk_dy, 0u, 0u };
    for (uint32_t mode = 0; mode < 2u; ++mode) {
        push[2] = mode;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command,
            (gpu->width * gpu->height + 255u) / 256u, 1u, 1u);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end velocity shift command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU velocity shift failed");
    return true;
}

static void record_projection_begin(dc_gpu_t *gpu) {
    uint32_t push[7] = { gpu->width, gpu->height, 0, 0, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    if (gpu->rigid_occupancy_present) {
        push[2]=FLUID_LATERAL_DISPLACEMENT_MODE;
        bind_fluid_mode(gpu,push[2],false);
        vkCmdPushConstants(gpu->command,gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(gpu->command,gpu->height<65535u?gpu->height:65535u,(gpu->height+65534u)/65535u,1u);
        fluid_barrier(gpu,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        push[2] = FLUID_DISPLACEMENT_MODE;
        bind_fluid_mode(gpu,push[2],false);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command,gpu->width<65535u?gpu->width:65535u,(gpu->width+65534u)/65535u,1u);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    bind_fluid_mode(gpu,5u,true);
    push[2] = 5u;
    push[3] = 1u;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 4u, 4u, DC_GPU_CHUNK_SLOTS);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->hydro_head_pipeline);
    for (uint32_t phase = 0; phase < 2; phase++) {
        push[2] = phase;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, gpu->width, 1u, 1u);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

static void record_pressure_passes(dc_gpu_t *gpu, uint32_t first,
                                    uint32_t stop) {
    uint32_t push[7] = { gpu->width, gpu->height, 0, 1, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    for (uint32_t pass = first; pass < stop; ++pass) {
        push[2] = pass == 0 ? 0u : pass == 2u * DC_PRESSURE_SWEEPS + 1u ? 3u :
                  ((pass & 1u) ? 1u : 2u);
        bind_fluid_mode(gpu,push[2],true);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, 4u, 4u, DC_GPU_CHUNK_SLOTS);
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

static uint32_t aperture_leaves(uint32_t length) {
    uint32_t leaves = 1;
    while (leaves < (length + 15u) / 16u) leaves *= 2;
    return leaves;
}

static void aperture_dispatch(dc_gpu_t *gpu, uint32_t groups) {
    vkCmdDispatch(gpu->command, groups < 65535u ? groups : 65535u,
                   (groups + 65534u) / 65535u, 1);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

static void record_aperture_cache(dc_gpu_t *gpu, bool vertical, uint32_t leaves) {
    uint32_t lines = vertical ? gpu->width : gpu->height;
    uint32_t push[8] = {gpu->width, gpu->height, vertical, 0, leaves};
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->fluid_aperture_pipeline);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    aperture_dispatch(gpu, lines * ((leaves + 3u) / 4u));
    for (uint32_t nodes = leaves / 2; nodes; nodes /= 2) {
        push[3] = nodes;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        aperture_dispatch(gpu, (lines * nodes + 63u) / 64u);
    }
}

static void record_fluid_transport(dc_gpu_t *gpu, uint32_t first,
                                   uint32_t stop) {
    uint32_t push[7] = { gpu->width, gpu->height, 0,
                          gpu->fluid_tick & 1u, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    push[6] = gpu->timed_fluid && gpu->fluid_step_scale > 1.0001f ? 1u : 0u;
    if (first == 0u) {
        push[2] = 10u;
        bind_fluid_mode(gpu,push[2],false);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u, 1u, DC_GPU_CHUNK_SLOTS);
        gpu->fluid_snapshot_valid = true;
        fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    for (uint32_t mode = first; mode < stop; ++mode) {
        if (mode < 4u) {
            bool vertical = mode < 2u;
            push[4] = gpu->rigid_occupancy_present ? aperture_leaves(vertical ? gpu->height : gpu->width) : 0;
            if (push[4] && mode != 1u) record_aperture_cache(gpu, vertical, push[4]);
        }
        push[2] = mode;
        bind_fluid_mode(gpu,push[2],false);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        uint32_t work = mode < 2u ? gpu->width : gpu->height;
        if (mode < 4u && gpu->rigid_occupancy_present)
            vkCmdDispatch(gpu->command, work < 65535u ? work : 65535u,
                           (work + 65534u) / 65535u, 1u);
        else if (mode < 4u)
            vkCmdDispatch(gpu->command, (work + 63u) / 64u, 1u, 1u);
        else
            vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                          1u, DC_GPU_CHUNK_SLOTS);
        if (mode < 4u)
            fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

static void record_fluid_correction(dc_gpu_t *gpu) {
    dc_gpu_record_markers(gpu);
    uint32_t push[7] = { gpu->width, gpu->height, 0,
                         gpu->fluid_tick & 1u, 0, 0, 0 };
    push[5] = dc_gpu_float_bits(gpu->fluid_step_scale);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    if (gpu->marker_correction) {
        const uint32_t correction_modes[6] = { 4u, 6u, 7u, 8u, 9u, 5u };
        for (uint32_t i = 0; i < 6; ++i) {
            push[2] = correction_modes[i];
            bind_fluid_mode(gpu,push[2],false);
            vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
            vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                          1u, DC_GPU_CHUNK_SLOTS);
            if (i < 5)
                fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }
    } else {
        push[2] = 5u;
        bind_fluid_mode(gpu,push[2],false);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, DC_CHUNK_CELLS / 64u,
                      1u, DC_GPU_CHUNK_SLOTS);
    }
    ++gpu->fluid_tick;
}

void dc_gpu_record_fluid_phase(dc_gpu_t *gpu, uint32_t phase) {
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    if (phase == 0u) record_projection_begin(gpu);
    const uint32_t pressure_stop = 2u * DC_PRESSURE_SWEEPS + 2u;
    if (phase == 0u) record_pressure_passes(gpu, 0u,
        pressure_stop < 10u ? pressure_stop : 10u);
    else if (phase == 1u) record_pressure_passes(gpu, 10u,
        pressure_stop < 25u ? pressure_stop : 25u);
    else if (phase == 2u) record_pressure_passes(gpu, 25u,
        pressure_stop < 40u ? pressure_stop : 40u);
    else if (phase == 3u) {
        record_pressure_passes(gpu, 40u, pressure_stop);
        record_fluid_transport(gpu, 0u, 3u);
    } else if (phase == 4u) record_fluid_transport(gpu, 3u, 5u);
    else if (phase == 5u) record_fluid_correction(gpu);
}

void dc_gpu_record_fluid(dc_gpu_t *gpu) {
    for (uint32_t phase = 0; phase < 6u; ++phase)
        dc_gpu_record_fluid_phase(gpu, phase);
}

bool dc_gpu_fluid_max_divergence(dc_gpu_t *gpu, float *divergence,
                                  char *err, uint32_t cap) {
    if (!gpu || !divergence) return error(err, cap, "Invalid fluid divergence diagnostic");
    if (!dc_gpu_copy_chunk_state(gpu, UINT32_MAX, false, false, err, cap)) return false;
    const uint32_t *pages = gpu->page_mapped;
    const dc_cell_t *cells = gpu->chunk_mapped;
    const float *faces = gpu->velocity_mapped;
    float maximum = 0.0f;
    for (uint32_t y = 1; y + 1 < gpu->height; ++y) {
        for (uint32_t x = 1; x + 1 < gpu->width; ++x) {
            uint32_t page = pages[(y / 64u) * gpu->page_width + x / 64u];
            if (!page) continue;
            uint32_t atlas = (page - 1u) * DC_CHUNK_CELLS +
                             (y % 64u) * 64u + x % 64u;
            if (cells[atlas].fluid_mass != DC_FLUID_FULL) continue;
            uint32_t index = y * gpu->width + x;
            float value = faces[2u * index] - faces[2u * (index - 1u)] +
                          faces[2u * index + 1u] -
                          faces[2u * (index - gpu->width) + 1u];
            if (value < 0.0f) value = -value;
            if (value > maximum) maximum = value;
        }
    }
    *divergence = maximum;
    return true;
}

bool dc_gpu_fluid_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset fluid command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin fluid command buffer");
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_fluid(gpu);
    fluid_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_HOST_READ_BIT);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end fluid command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "GPU fluid submission failed");
    return true;
}
