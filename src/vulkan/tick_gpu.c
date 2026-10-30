#include <stdio.h>
#include <stdlib.h>

#include "gpu_internal.h"
#include "gas_gpu_internal.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "runtime_body_gpu.h"
#include "thermal_gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static bool make_probe_pipeline(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/tick_probe.comp.spv",
                                   &module, err, cap)) return false;
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, gpu->pipeline_cache, 1,
                                               &pipeline_info, NULL, &gpu->probe_pipeline);
    vkDestroyShaderModule(gpu->device, module, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create tick probe pipeline");
    return true;
}

bool dc_gpu_tick_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &count, NULL);
    if (gpu->family >= count) return error(err, cap, "Invalid timestamp queue family");
    VkQueueFamilyProperties *families = calloc(count, sizeof(*families));
    if (!families) return error(err, cap, "Out of memory checking GPU timestamps");
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &count, families);
    gpu->timestamp_bits = families[gpu->family].timestampValidBits;
    free(families);
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical, &properties);
    gpu->timestamp_period = properties.limits.timestampPeriod;
    if (!gpu->timestamp_bits || gpu->timestamp_period <= 0.0f)
        return error(err, cap, "Compute queue lacks timestamp queries");
    VkQueryPoolCreateInfo query_info = { .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 34 };
    if (vkCreateQueryPool(gpu->device, &query_info, NULL, &gpu->timestamp_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create GPU timestamp pool");
    VkFenceCreateInfo fence_info={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(vkCreateFence(gpu->device,&fence_info,NULL,&gpu->tick_fence)!=VK_SUCCESS)
        return error(err,cap,"Cannot create GPU tick completion fence");
    return make_probe_pipeline(gpu, err, cap) &&
           dc_gpu_fluid_pipeline_init(gpu, err, cap) &&
           dc_gpu_marker_pipeline_init(gpu, err, cap) &&
           dc_gpu_mpm_pipeline_init(gpu, err, cap);
}

void dc_gpu_tick_destroy(dc_gpu_t *gpu) {
    if(gpu->tick_fence) vkDestroyFence(gpu->device,gpu->tick_fence,NULL);
    if (gpu->probe_pipeline) vkDestroyPipeline(gpu->device, gpu->probe_pipeline, NULL);
    dc_gpu_mpm_pipeline_destroy(gpu);
    if (gpu->timestamp_pool) vkDestroyQueryPool(gpu->device, gpu->timestamp_pool, NULL);
    if (gpu->trace_mapped) vkUnmapMemory(gpu->device, gpu->trace_memory);
    if (gpu->trace_buffer) vkDestroyBuffer(gpu->device, gpu->trace_buffer, NULL);
    if (gpu->trace_memory) vkFreeMemory(gpu->device, gpu->trace_memory, NULL);
}

static void stage_barrier(dc_gpu_t *gpu, VkPipelineStageFlags2 dst_stage,
                          VkAccessFlags2 dst_access) {
    VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = dst_stage, .dstAccessMask = dst_access };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

static void record_scheduled_fluid(dc_gpu_t *gpu) {
    if (gpu->fluid_interval == 1u) {
        gpu->fluid_step_scale = gpu->timed_fluid ? gpu->tick_time_scale : 1.0f;
        if (gpu->fluid_work_possible) dc_gpu_record_fluid(gpu);
        else ++gpu->fluid_tick;
        return;
    }
    gpu->fluid_step_scale = gpu->timed_fluid ? (float)gpu->fluid_interval : 1.0f;
    gpu->fluid_phase_budget += (gpu->timed_fluid ? gpu->tick_time_scale : 1.0f) *
                               (6.0f / (float)gpu->fluid_interval);
    while (gpu->fluid_phase_budget >= 1.0f) {
        if (gpu->fluid_work_possible) dc_gpu_record_fluid_phase(gpu, gpu->fluid_phase);
        else if (gpu->fluid_phase == 5u) ++gpu->fluid_tick;
        gpu->fluid_phase = (gpu->fluid_phase + 1u) % 6u;
        gpu->fluid_phase_budget -= 1.0f;
    }
}

static void record_scheduled_rigid(dc_gpu_t *gpu) {
    gpu->rigid_step_budget += gpu->timed_fluid ? gpu->tick_time_scale : 1.0;
    uint32_t steps = 0;
    bool profile = gpu->component_profile;
    while (gpu->rigid_step_budget + 1e-6 >= 1.0) {
        /* Detailed query slots are written once per displayed frame. */
        if (steps) gpu->component_profile = false;
        dc_gpu_record_rigid(gpu);
        gpu->rigid_step_budget -= 1.0;
        steps++;
    }
    gpu->component_profile = profile;
    if (gpu->rigid_step_budget < 0.0) gpu->rigid_step_budget = 0.0;
    if (steps == 0) dc_gpu_record_rigid_refresh(gpu);
}

void dc_gpu_record_tick_step(dc_gpu_t *gpu) {
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_tick_water_source(gpu);
    record_scheduled_rigid(gpu);
    record_scheduled_fluid(gpu);
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_mpm(gpu);
    fr_thermal_record_step(gpu,(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f);
}

static void record_probe(dc_gpu_t *gpu, uint32_t mode) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->probe_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, mode, dc_gpu_runtime_body_count(gpu), 0, 0, 0 };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1, 1, 1);
}

static uint64_t elapsed_ns(const dc_gpu_t *gpu, uint64_t start, uint64_t stop) {
    uint64_t mask = gpu->timestamp_bits == 64 ? UINT64_MAX :
                    (UINT64_C(1) << gpu->timestamp_bits) - 1;
    uint64_t ticks = (stop - start) & mask;
    return (uint64_t)((double)ticks * gpu->timestamp_period);
}

static bool tick_submit(dc_gpu_t *gpu, dc_gpu_tick_capture_t *capture,
                        char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "Invalid GPU tick");
    if(!dc_gpu_runtime_bodies_service(gpu,err,cap)) return false;
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset tick command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin tick command buffer");
    if (capture) {
        vkCmdResetQueryPool(gpu->command, gpu->timestamp_pool, 0, 34);
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             gpu->timestamp_pool, 0);
    }
    dc_gpu_record_tick_water_source(gpu);
    record_scheduled_rigid(gpu);
    if (capture) {
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 1);
        record_probe(gpu, 0);
    }
    record_scheduled_fluid(gpu);
    if (capture) {
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 2);
    }
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_mpm(gpu);
    fr_thermal_record_step(gpu,(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f);
    if (capture) {
        record_probe(gpu, 1);
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 3);
    }
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end tick command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkResetFences(gpu->device,1,&gpu->tick_fence)!=VK_SUCCESS||
        vkQueueSubmit(gpu->queue, 1, &submit, gpu->tick_fence) != VK_SUCCESS ||
        vkWaitForFences(gpu->device,1,&gpu->tick_fence,VK_TRUE,UINT64_MAX) != VK_SUCCESS)
        return error(err, cap, "GPU tick submission failed");
    if (!capture) return true;
    uint64_t timestamps[4] = {0};
    if (vkGetQueryPoolResults(gpu->device, gpu->timestamp_pool, 0, 4,
            sizeof(timestamps), timestamps, sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
        return error(err, cap, "Cannot read GPU timestamps");
    const uint32_t *trace = gpu->trace_mapped;
    for (uint32_t i = 0; i < 3; ++i) {
        capture->stages[i].id = (dc_gpu_stage_id_t)(i + 1);
        capture->stages[i].gpu_ns = elapsed_ns(gpu, timestamps[i], timestamps[i + 1]);
        capture->stages[i].handoff = trace[i];
    }
    return true;
}

bool dc_gpu_tick_capture(dc_gpu_t *gpu, dc_gpu_tick_capture_t *capture,
                         char *err, uint32_t cap) {
    if (!capture) return error(err, cap, "Invalid GPU tick capture");
    if(!tick_submit(gpu,capture,err,cap)) return false;
    return !gpu->gas||fr_gas_gpu_step(gpu,(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f,err,cap);
}

bool dc_gpu_tick_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if(gpu&&fr_combustion_gpu_enabled(gpu)&&getenv("DC_FRAME_TIMINGS")) {
        dc_gpu_tick_capture_t capture;
        if(!tick_submit(gpu,&capture,err,cap)) return false;
        fprintf(stderr,"Reactive tick GPU rigid%.3f fluid%.3f mpm_heat%.3f ms\n",
            capture.stages[0].gpu_ns/1e6,capture.stages[1].gpu_ns/1e6,capture.stages[2].gpu_ns/1e6);
        return !gpu->gas||fr_gas_gpu_step(gpu,(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f,err,cap);
    }
    bool handled=false;
    if(!fr_gas_gpu_try_fused_tick(gpu,gpu?(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f:0.f,&handled,err,cap)) return false;
    if(handled) return true;
    if(!tick_submit(gpu,NULL,err,cap)) return false;
    return !gpu->gas||fr_gas_gpu_step(gpu,(gpu->timed_fluid?gpu->tick_time_scale:1.f)/60.f,err,cap);
}

bool dc_gpu_set_fluid_interval(dc_gpu_t *gpu, uint32_t interval) {
    if (!gpu || (interval != 1u && interval != 2u &&
                 interval != 3u && interval != 6u)) return false;
    gpu->fluid_interval = interval;
    gpu->fluid_phase = 0u;
    gpu->fluid_phase_budget = 0.0f;
    gpu->fluid_step_scale = 1.0f;
    gpu->fluid_snapshot_valid = false;
    return true;
}

bool dc_gpu_set_tick_seconds(dc_gpu_t *gpu, float seconds) {
    if (!gpu || !(seconds > 0.0f) || seconds > 0.25f) return false;
    gpu->tick_time_scale = seconds * 60.0f;
    gpu->timed_fluid = true;
    return true;
}
