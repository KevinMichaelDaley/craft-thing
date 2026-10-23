/* Explicit experiment binary only; ordinary runtime pipelines stay unchanged. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"

static uint32_t selected_modes;
static dc_gpu_t *variant_gpu;
static VkPipeline pipelines[8];
static bool production, specialized, bound_mpm;

void __real_dc_gpu_record_mpm(dc_gpu_t *gpu);
void __real_dc_gpu_destroy(dc_gpu_t *gpu);
void __real_vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void __real_vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags,
                               uint32_t, uint32_t, const void *);

static void prepare(dc_gpu_t *gpu) {
    const char *choice = getenv("DC_MPM_VARIANTS");
    if (!choice) choice = "generic";
    production = strcmp(choice, "production") == 0;
    specialized = strcmp(choice, "specialized") == 0;
    if (!production && !specialized && strcmp(choice, "generic") != 0) {
        fputs("Invalid diagnostic MPM variant\n", stderr);
        exit(2);
    }
    variant_gpu = gpu;
    selected_modes = 0;
    if (production) return;
    char err[256];
    VkShaderModule module;
    if (!dc_gpu_load_shader_module(gpu, "build/diagnostics/fluid-variants/mpm.spv",
                                   &module, err, sizeof(err))) {
        fprintf(stderr, "MPM variant module: %s\n", err);
        exit(2);
    }
    unsigned count = specialized ? 8u : 1u;
    for (unsigned i = 0; i < count; i++) {
        uint32_t mode = specialized ? i : UINT32_MAX;
        VkSpecializationMapEntry entry = {.constantID = 0, .size = sizeof(mode)};
        VkSpecializationInfo constants = {.mapEntryCount = 1, .pMapEntries = &entry,
                                          .dataSize = sizeof(mode), .pData = &mode};
        VkComputePipelineCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module,
                      .pName = "main", .pSpecializationInfo = &constants},
            .layout = gpu->pipeline_layout};
        VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE,
                                                    1, &info, NULL, &pipelines[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "MPM variant phase%u creation status%d\n", mode, result);
            exit(2);
        }
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    printf("diagnostic MPM %s: %u pipelines\n", choice, count);
}

void __wrap_dc_gpu_record_mpm(dc_gpu_t *gpu) {
    gpu->solver_cache_disabled=true;
    if (variant_gpu != gpu) prepare(gpu);
    __real_dc_gpu_record_mpm(gpu);
}

void __wrap_vkCmdBindPipeline(VkCommandBuffer command, VkPipelineBindPoint point,
                              VkPipeline pipeline) {
    if (variant_gpu && command == variant_gpu->command && point == VK_PIPELINE_BIND_POINT_COMPUTE) {
        bound_mpm = !production && pipeline == variant_gpu->mpm_pipeline;
        for(unsigned phase=0;!production&&phase<8u;phase++)
            bound_mpm=bound_mpm||pipeline==variant_gpu->mpm_phase_pipelines[phase];
    }
    __real_vkCmdBindPipeline(command, point, pipeline);
}

void __wrap_vkCmdPushConstants(VkCommandBuffer command, VkPipelineLayout layout,
                               VkShaderStageFlags flags, uint32_t offset,
                               uint32_t size, const void *values) {
    __real_vkCmdPushConstants(command, layout, flags, offset, size, values);
    if (!variant_gpu || command != variant_gpu->command || !bound_mpm || offset != 0 || size < 12) return;
    uint32_t mode;
    memcpy(&mode, (const char *)values + 8, sizeof(mode));
    if (mode >= 8) {
        fputs("Unhandled diagnostic MPM phase\n", stderr);
        exit(2);
    }
    selected_modes |= 1u << mode;
    __real_vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                             pipelines[specialized ? mode : 0]);
}

void __wrap_dc_gpu_destroy(dc_gpu_t *gpu) {
    if (gpu && variant_gpu == gpu) {
        vkDeviceWaitIdle(gpu->device);
        for (unsigned i = 0; i < 8; i++) {
            if (pipelines[i]) vkDestroyPipeline(gpu->device, pipelines[i], NULL);
            pipelines[i] = VK_NULL_HANDLE;
        }
        variant_gpu = NULL;
        bound_mpm = false;
    }
    __real_dc_gpu_destroy(gpu);
}

uint32_t diagnostic_mpm_selected_modes(void) {
    return selected_modes;
}
