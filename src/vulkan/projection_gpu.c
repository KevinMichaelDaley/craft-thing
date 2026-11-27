#include "gpu_internal.h"
#include "renderer_internal.h"

enum { PRESSURE_GROUP_SIDE = 16u };

static void bind_projection(dc_gpu_t *gpu, uint32_t mode, const VkDescriptorSet sets[2]) {
    VkPipeline pipeline = gpu->projection_phase_pipelines[mode];
    if (!pipeline) pipeline = gpu->projection_pipeline;
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                           gpu->pressure_pipeline_layout, 0, 2, sets, 0, NULL);
}

static void projection_barrier(dc_gpu_t *gpu) {
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

static void record_hydro_head(dc_gpu_t *gpu, VkDescriptorSet descriptor,
                              uint32_t width, uint32_t height) {
    uint32_t push[7] = {width, height};
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->hydro_head_pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &descriptor, 0, NULL);
    for (uint32_t phase = 0; phase < 2; ++phase) {
        push[2] = phase;
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        vkCmdDispatch(gpu->command, width, 1u, 1u);
        projection_barrier(gpu);
    }
}

static void record_pressure_stencil(dc_gpu_t *gpu) {
    const uint32_t push[7] = {gpu->width, gpu->height, 6u, 0u, 1u, 0u, 0u};
    const VkDescriptorSet sets[] = {gpu->descriptor, gpu->pressure_work_descriptor};
    bind_projection(gpu, push[2], sets);
    vkCmdPushConstants(gpu->command, gpu->pressure_pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatchIndirect(gpu->command, gpu->pressure_work_buffer, sizeof(uint32_t));
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
}

static uint32_t pressure_phase(uint32_t pass) {
    if (pass == 0u) return 0u;
    if (pass == DC_PRESSURE_PASS_COUNT - 1u) return 3u;
    return (pass & 1u) ? 1u : 2u;
}

static void record_projection_pass(dc_gpu_t *gpu, const VkDescriptorSet sets[2],
                                   const uint32_t push[7], uint32_t slots) {
    bind_projection(gpu, push[2], sets);
    vkCmdPushConstants(gpu->command, gpu->pressure_pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, 7u * sizeof(uint32_t), push);
    if (push[4]) vkCmdDispatchIndirect(gpu->command, gpu->pressure_work_buffer, sizeof(uint32_t));
    else if (push[3]) vkCmdDispatch(gpu->command,
        DC_CHUNK_SIDE / PRESSURE_GROUP_SIDE, DC_CHUNK_SIDE / PRESSURE_GROUP_SIDE, slots);
    else vkCmdDispatch(gpu->command,
        (push[0] + PRESSURE_GROUP_SIDE - 1u) / PRESSURE_GROUP_SIDE,
        (push[1] + PRESSURE_GROUP_SIDE - 1u) / PRESSURE_GROUP_SIDE, 1u);
    projection_barrier(gpu);
}

void dc_gpu_record_projection_fields(dc_gpu_t *gpu) {
    const VkDescriptorSet sets[] = {gpu->descriptor, gpu->pressure_work_descriptor};
    const uint32_t push[7] = {gpu->width, gpu->height, 5u, 1u, 0u,
                              dc_gpu_float_bits(gpu->fluid_step_scale), 0u};
    record_projection_pass(gpu, sets, push, dc_gpu_slot_capacity(gpu));
    record_hydro_head(gpu, gpu->descriptor, gpu->width, gpu->height);
}

void dc_gpu_record_dense_projection(dc_gpu_t *gpu, const VkDescriptorSet sets[2],
    uint32_t width, uint32_t height, float step_scale, uint32_t first, uint32_t stop) {
    uint32_t push[7] = {width, height, 5u, 0u, 0u, dc_gpu_float_bits(step_scale), 0u};
    if (first == 0u) {
        record_projection_pass(gpu, sets, push, 0u);
        record_hydro_head(gpu, sets[0], width, height);
    }
    for (uint32_t pass = first; pass < stop; ++pass) {
        push[2] = pressure_phase(pass);
        record_projection_pass(gpu, sets, push, 0u);
    }
}

void dc_gpu_fluid_pressure_range(uint32_t phase, uint32_t *first, uint32_t *stop) {
    const uint32_t cuts[] = {0u, 10u, 25u, 40u, DC_PRESSURE_PASS_COUNT};
    *first = cuts[phase] < DC_PRESSURE_PASS_COUNT ? cuts[phase] : DC_PRESSURE_PASS_COUNT;
    *stop = cuts[phase + 1u] < DC_PRESSURE_PASS_COUNT ? cuts[phase + 1u] : DC_PRESSURE_PASS_COUNT;
}

void dc_gpu_record_pressure_passes(dc_gpu_t *gpu, uint32_t first,
                                    uint32_t stop) {
    if (first >= stop) return;
    if (gpu->pressure_work_enabled) dc_gpu_record_pressure_work(gpu);
    if (gpu->pressure_work_enabled && first != 0u) record_pressure_stencil(gpu);
    const VkDescriptorSet sets[] = {gpu->descriptor, gpu->pressure_work_descriptor};
    uint32_t push[7] = {gpu->width, gpu->height, 0u, 1u, 0u,
                        dc_gpu_float_bits(gpu->fluid_step_scale), 0u};
    for (uint32_t pass = first; pass < stop; ++pass) {
        push[2] = pressure_phase(pass);
        bool sparse = gpu->pressure_work_enabled && (push[2] == 1u || push[2] == 2u);
        push[3] = !sparse;
        push[4] = sparse;
        push[6] = sparse;
        record_projection_pass(gpu, sets, push, dc_gpu_slot_capacity(gpu));
        if (gpu->pressure_work_enabled && pass == 0u) record_pressure_stencil(gpu);
    }
}
