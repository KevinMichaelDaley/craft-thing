/* Link only into the explicit diagnostic binary. Ordinary frames have no hooks. */
#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

enum { TRACE_DISPATCHES = 256, TRACE_LABELS = 23 };
static dc_gpu_t *trace_gpu;
static VkQueryPool trace_pool;
static VkPipeline bound_pipeline;
static uint32_t pushed_mode, trace_count, labels[TRACE_DISPATCHES];
static bool overflow;

VkResult __real_vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo *);
void __real_vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void __real_vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags,
                               uint32_t, uint32_t, const void *);
void __real_vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
bool __real_dc_gpu_tick_capture(dc_gpu_t *, dc_gpu_tick_capture_t *, char *, uint32_t);

VkResult __wrap_vkBeginCommandBuffer(VkCommandBuffer command,
                                     const VkCommandBufferBeginInfo *begin) {
    VkResult result = __real_vkBeginCommandBuffer(command, begin);
    if (result == VK_SUCCESS && trace_gpu && command == trace_gpu->command) {
        trace_count = 0;
        bound_pipeline = VK_NULL_HANDLE;
        vkCmdResetQueryPool(command, trace_pool, 0, 2 * TRACE_DISPATCHES);
    }
    return result;
}

void __wrap_vkCmdBindPipeline(VkCommandBuffer command, VkPipelineBindPoint point,
                              VkPipeline pipeline) {
    if (trace_gpu && command == trace_gpu->command && point == VK_PIPELINE_BIND_POINT_COMPUTE)
        bound_pipeline = pipeline;
    __real_vkCmdBindPipeline(command, point, pipeline);
}

void __wrap_vkCmdPushConstants(VkCommandBuffer command, VkPipelineLayout layout,
                               VkShaderStageFlags flags, uint32_t offset,
                               uint32_t size, const void *values) {
    if (trace_gpu && command == trace_gpu->command && offset == 0 && size >= 12)
        memcpy(&pushed_mode, (const char *)values + 8, sizeof(pushed_mode));
    __real_vkCmdPushConstants(command, layout, flags, offset, size, values);
}

static uint32_t label(void) {
    if (pushed_mode < 13 && ((pushed_mode < 4 && bound_pipeline == trace_gpu->fluid_transport_pipelines[pushed_mode]) ||
                             bound_pipeline == trace_gpu->fluid_pipeline ||
                             bound_pipeline == trace_gpu->fluid_phase_pipelines[pushed_mode]))
        return pushed_mode;
    if (pushed_mode < 6 && (bound_pipeline == trace_gpu->projection_pipeline ||
                            bound_pipeline == trace_gpu->projection_phase_pipelines[pushed_mode]))
        return 13 + pushed_mode;
    if (bound_pipeline == trace_gpu->hydro_head_pipeline && pushed_mode < 2)
        return 19 + pushed_mode;
    if (bound_pipeline == trace_gpu->fluid_aperture_pipeline && pushed_mode < 2)
        return 21 + pushed_mode;
    return TRACE_LABELS;
}

void __wrap_vkCmdDispatch(VkCommandBuffer command, uint32_t x, uint32_t y, uint32_t z) {
    uint32_t index = trace_gpu && command == trace_gpu->command ? label() : TRACE_LABELS;
    bool capture = index < TRACE_LABELS && trace_count < TRACE_DISPATCHES;
    if (index < TRACE_LABELS && !capture) overflow = true;
    if (capture)
        vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                              trace_pool, trace_count * 2);
    __real_vkCmdDispatch(command, x, y, z);
    if (capture) {
        vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                              trace_pool, trace_count * 2 + 1);
        labels[trace_count++] = index;
    }
}

bool __wrap_dc_gpu_tick_capture(dc_gpu_t *gpu, dc_gpu_tick_capture_t *capture,
                                char *err, uint32_t cap) {
    if (!gpu->component_profile)
        return __real_dc_gpu_tick_capture(gpu, capture, err, cap);
    VkQueryPoolCreateInfo info = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                  .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                  .queryCount = 2 * TRACE_DISPATCHES};
    if (vkCreateQueryPool(gpu->device, &info, NULL, &trace_pool) != VK_SUCCESS) {
        if (err && cap) snprintf(err, cap, "Cannot allocate diagnostic fluid timestamps");
        return false;
    }
    trace_gpu = gpu;
    overflow = false;
    uint32_t phase = gpu->fluid_phase;
    bool result = __real_dc_gpu_tick_capture(gpu, capture, err, cap);
    trace_gpu = NULL;
    uint64_t times[2 * TRACE_DISPATCHES];
    if (result && trace_count) {
        result = vkGetQueryPoolResults(gpu->device, trace_pool, 0, trace_count * 2,
                                       trace_count * 2 * sizeof(uint64_t), times,
                                       sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
        if (result) {
            double total[TRACE_LABELS] = {0};
            unsigned count[TRACE_LABELS] = {0};
            for (unsigned i = 0; i < trace_count; i++) {
                total[labels[i]] += (times[i * 2 + 1] - times[i * 2]) * gpu->timestamp_period / 1e6;
                count[labels[i]]++;
            }
            printf("fluid dispatch detail phase%u:", phase);
            for (unsigned i = 0; i < TRACE_LABELS; i++)
                if (count[i]) printf(" %s%u=%.3fms/%u", i < 13 ? "fluid" : i < 19 ? "projection" : i < 21 ? "head" : "aperture",
                                      i < 13 ? i : i < 19 ? i - 13 : i < 21 ? i - 19 : i - 21, total[i], count[i]);
            printf("\n");
        }
    }
    vkDestroyQueryPool(gpu->device, trace_pool, NULL);
    trace_pool = VK_NULL_HANDLE;
    return result && !overflow;
}
