#include <stdio.h>
#include <stdlib.h>

#include "gpu_internal.h"

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
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1,
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
        .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 4 };
    if (vkCreateQueryPool(gpu->device, &query_info, NULL, &gpu->timestamp_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create GPU timestamp pool");
    return make_probe_pipeline(gpu, err, cap) &&
           dc_gpu_fluid_pipeline_init(gpu, err, cap);
}

void dc_gpu_tick_destroy(dc_gpu_t *gpu) {
    if (gpu->probe_pipeline) vkDestroyPipeline(gpu->device, gpu->probe_pipeline, NULL);
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

void dc_gpu_record_tick_step(dc_gpu_t *gpu) {
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_tick_water_source(gpu);
    dc_gpu_record_rigid(gpu);
    dc_gpu_record_fluid(gpu);
}

static void record_probe(dc_gpu_t *gpu, uint32_t mode) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->probe_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, mode, 0, 0, 0, 0 };
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
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset tick command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin tick command buffer");
    if (capture) {
        vkCmdResetQueryPool(gpu->command, gpu->timestamp_pool, 0, 4);
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             gpu->timestamp_pool, 0);
    }
    dc_gpu_record_tick_water_source(gpu);
    dc_gpu_record_rigid(gpu);
    if (capture) {
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 1);
        record_probe(gpu, 0);
    }
    dc_gpu_record_fluid(gpu);
    if (capture) {
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 2);
        stage_barrier(gpu, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        record_probe(gpu, 1);
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             gpu->timestamp_pool, 3);
    }
    stage_barrier(gpu, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end tick command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
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
    return tick_submit(gpu, capture, err, cap);
}

bool dc_gpu_tick_step(dc_gpu_t *gpu, char *err, uint32_t cap) {
    return tick_submit(gpu, NULL, err, cap);
}
