#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../src/vulkan/gpu_internal.h"

int fr_diag_forbid_submit;
static struct {
    VkShaderModule module;
    char path[256];
} modules[64];
static unsigned module_count;
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
bool __real_dc_gpu_load_shader_module(dc_gpu_t *, const char *,
                                      VkShaderModule *, char *, uint32_t);
bool __wrap_dc_gpu_load_shader_module(dc_gpu_t *gpu, const char *path,
                                      VkShaderModule *module, char *err,
                                      uint32_t cap) {
    fprintf(stderr, "shader load begin %s\n", path);
    fflush(stderr);
    bool ok = __real_dc_gpu_load_shader_module(gpu, path, module, err, cap);
    if (ok) {
        unsigned index = module_count;
        for (unsigned i = 0; i < module_count; i++)
            if (modules[i].module == *module) {
                index = i;
                break;
            }
        if (index < 64) {
            modules[index].module = *module;
            snprintf(modules[index].path, sizeof(modules[index].path), "%s",
                     path);
            if (index == module_count)
                module_count++;
        }
    }
    fprintf(stderr, "shader load end %s success%d\n", path, ok);
    fflush(stderr);
    return ok;
}
VKAPI_ATTR VkResult VKAPI_CALL __real_vkCreateComputePipelines(
    VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo *,
    const VkAllocationCallbacks *, VkPipeline *);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkCreateComputePipelines(
    VkDevice device, VkPipelineCache cache, uint32_t count,
    const VkComputePipelineCreateInfo *info,
    const VkAllocationCallbacks *allocation, VkPipeline *pipelines) {
    const char *path = "unknown";
    uint32_t phase = UINT32_MAX;
    if (count) {
        for (unsigned i = 0; i < module_count; i++)
            if (modules[i].module == info[0].stage.module)
                path = modules[i].path;
        const VkSpecializationInfo *special = info[0].stage.pSpecializationInfo;
        if (special)
            for (unsigned i = 0; i < special->mapEntryCount; i++) {
                const VkSpecializationMapEntry *entry =
                    special->pMapEntries + i;
                if (entry->constantID == 0 && entry->size == sizeof(phase) &&
                    entry->offset + sizeof(phase) <= special->dataSize)
                    memcpy(&phase, (const char *)special->pData + entry->offset,
                           sizeof(phase));
            }
    }
    double start = now();
    fprintf(stderr, "pipeline begin %s phase%u\n", path, phase);
    fflush(stderr);
    VkResult result = __real_vkCreateComputePipelines(
        device, cache, count, info, allocation, pipelines);
    fprintf(stderr, "pipeline end %s phase%u status%d %.3fms\n", path, phase,
            result, (now() - start) * 1000.);
    fflush(stderr);
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL __real_vkQueueSubmit(VkQueue, uint32_t,
                                                    const VkSubmitInfo *,
                                                    VkFence);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit(VkQueue queue,
                                                    uint32_t count,
                                                    const VkSubmitInfo *submit,
                                                    VkFence fence) {
    fprintf(stderr, "engine queue submit begin count%u forbidden%d\n", count,
            fr_diag_forbid_submit);
    fflush(stderr);
    if (fr_diag_forbid_submit)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = __real_vkQueueSubmit(queue, count, submit, fence);
    fprintf(stderr, "engine queue submit end status%d\n", result);
    fflush(stderr);
    return result;
}
