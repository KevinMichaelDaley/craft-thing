#include "dungeoncraft/fluid_aperture_layout.h"
#include "gpu_internal.h"
#include "renderer_internal.h"

void dc_gpu_record_water_displacement(dc_gpu_t *gpu, VkDescriptorSet descriptor,
    const VkPipeline pipelines[2], uint32_t width, uint32_t height, uint32_t seam_axis) {
    enum { UPWARD = 0u, LATERAL = 1u, FIRST_DISPLACEMENT_MODE = 11u };
    const uint32_t order[] = {LATERAL, UPWARD};
    uint32_t push[4] = {width, height, 0u, seam_axis};
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &descriptor, 0, NULL);
    for (uint32_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        const uint32_t mode = order[i];
        const uint32_t lines = mode == LATERAL ? height : width;
        push[2] = FIRST_DISPLACEMENT_MODE + mode;
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[mode]);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, lines < 65535u ? lines : 65535u, (lines + 65534u) / 65535u, 1u);
        fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
}

uint32_t dc_gpu_fluid_aperture_leaves(uint32_t length) {
    uint32_t leaves = 1;
    while (leaves < (length + DC_FLUID_APERTURE_TILE_CELLS - 1u) / DC_FLUID_APERTURE_TILE_CELLS)
        leaves *= 2;
    return leaves;
}

static void aperture_dispatch(dc_gpu_t *gpu, uint32_t groups) {
    vkCmdDispatch(gpu->command, groups < 65535u ? groups : 65535u,
                   (groups + 65534u) / 65535u, 1);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

static void record_aperture_cache(dc_gpu_t *gpu, VkDescriptorSet descriptor,
                                  uint32_t width, uint32_t height, bool vertical, uint32_t leaves) {
    uint32_t lines = vertical ? width : height;
    uint32_t push[8] = {width, height, vertical, 0, leaves};
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->fluid_aperture_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    aperture_dispatch(gpu, lines * ((leaves + 3u) / 4u));
    for (uint32_t nodes = leaves / 2; nodes; nodes /= 2) {
        push[3] = nodes;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        aperture_dispatch(gpu, (lines * nodes + 63u) / 64u);
    }
}

void dc_gpu_record_transport_sweep(dc_gpu_t *gpu, VkDescriptorSet descriptor, VkPipeline pipeline,
                                   uint32_t *push, uint32_t push_bytes, bool occupancy) {
    const bool vertical = push[2] < 2u;
    push[4] = occupancy ? dc_gpu_fluid_aperture_leaves(vertical ? push[1] : push[0]) : 0u;
    if (push[4] && push[2] != 1u)
        record_aperture_cache(gpu, descriptor, push[0], push[1], vertical, push[4]);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, push_bytes, push);
    aperture_dispatch(gpu, vertical ? push[0] : push[1]);
}

void dc_gpu_record_water_seam_transport(dc_gpu_t *gpu, VkDescriptorSet descriptor,
    const VkPipeline pipelines[4], uint32_t width, uint32_t height,
    float step_scale, bool occupancy, uint32_t axis, uint32_t first, uint32_t stop) {
    uint32_t push[8] = {width, height, 0, 0, 0, dc_gpu_float_bits(step_scale), step_scale > 1.0001f, axis};
    for (uint32_t mode = first; mode < stop; ++mode) {
        push[2] = mode;
        dc_gpu_record_transport_sweep(gpu, descriptor, pipelines[mode], push, sizeof(push), occupancy);
    }
}
