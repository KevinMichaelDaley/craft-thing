#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gpu_internal.h"

enum { SOLVER_ITERATIONS = 8, SOLVER_SUBSTEPS = 8, SOLVER_DISPATCH_WORD = 6 };
typedef enum {
    SOLVER_RESET,
    SOLVER_BEGIN_FRAME,
    SOLVER_PREDICT,
    SOLVER_PREPARE,
    SOLVER_INIT_CONTACTS,
    SOLVER_CLEAR,
    SOLVER_SOLVE,
    SOLVER_APPLY,
    SOLVER_DERIVE_VELOCITY,
    SOLVER_IMPULSE,
    SOLVER_APPLY_VELOCITY,
    SOLVER_FINISH
} solver_phase_t;
_Static_assert(SOLVER_FINISH + 1 == DC_GPU_SOLVER_PHASE_COUNT, "GPU solver phase count changed");
static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap)
        snprintf(err, cap, "%s", message);
    return false;
}
static dc_gpu_body_record_t *find_body(dc_gpu_t *gpu, uint32_t id) {
    if (!gpu || !id)
        return NULL;
    for (uint32_t i = 0; i < gpu->body_count; ++i)
        if (gpu->body_ids[i] == id)
            return &((dc_gpu_body_record_t *)gpu->body_mapped)[i];
    return NULL;
}
bool dc_gpu_set_rigid_solver(dc_gpu_t *gpu, bool enabled) {
    if (!gpu)
        return false;
    gpu->rigid_solver_enabled = enabled;
    uint32_t *data = dc_gpu_solver_data(gpu);
    data[0] = enabled;
    data[1] = SOLVER_ITERATIONS;
    data[2] = SOLVER_SUBSTEPS;
    return true;
}
bool dc_gpu_set_body_motion(dc_gpu_t *gpu, uint32_t id, float angle, float omega, uint32_t flags,
                            char *err, uint32_t cap) {
    if (!isfinite(angle) || !isfinite(omega) || fabsf(omega) > .5f || (flags & ~3u) ||
        fabsf(angle) > 1000000.f)
        return error(err, cap, "Invalid rigid motion");
    dc_gpu_body_record_t *b = find_body(gpu, id);
    if (!b)
        return error(err, cap, "GPU body ID not found");
    const float pi = 3.14159265358979323846f, turn = 2.f * pi;
    angle -= (float)(int)(angle / turn) * turn;
    if (angle > pi)
        angle -= turn;
    if (angle < -pi)
        angle += turn;
    b->angle = angle;
    b->angular_velocity = omega;
    b->motion_flags = flags;
    b->motion_ready = 0;
    gpu->body_refresh_pending = true;
    return true;
}
bool dc_gpu_read_body_motion(dc_gpu_t *gpu, uint32_t id, dc_gpu_body_motion_t *motion, char *err,
                             uint32_t cap) {
    dc_gpu_body_record_t *b = find_body(gpu, id);
    if (!b || !motion)
        return error(err, cap, "Invalid rigid motion readback");
    *motion =
        (dc_gpu_body_motion_t){b->angle,   b->angular_velocity, b->mass,      b->inertia,
                               b->density, b->center[0],        b->center[1], b->motion_flags};
    return true;
}
bool dc_gpu_read_rigid_solver_stats(dc_gpu_t *gpu, dc_gpu_rigid_solver_stats_t *stats) {
    if (!gpu || !stats)
        return false;
    memcpy(stats, dc_gpu_solver_data(gpu), sizeof(*stats));
    return true;
}
bool dc_gpu_solver_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/rigid_solver.comp.spv", &module, err, cap))
        return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main"},
        .layout = gpu->pipeline_layout};
    VkResult result = VK_SUCCESS;
    VkSpecializationMapEntry entry = {.constantID = 0, .offset = 0, .size = sizeof(uint32_t)};
    for (uint32_t mode = 0; mode < DC_GPU_SOLVER_PHASE_COUNT && result == VK_SUCCESS; ++mode) {
        VkSpecializationInfo phase = {
            .mapEntryCount = 1, .pMapEntries = &entry, .dataSize = sizeof(mode), .pData = &mode};
        info.stage.pSpecializationInfo = &phase;
        result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &info, NULL,
                                          &gpu->solver_pipelines[mode]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS ? true : error(err, cap, "Cannot create GPU rigid solver pipeline");
}
static void dispatch(dc_gpu_t *gpu, uint32_t mode, bool indirect) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->solver_pipelines[mode]);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width,  gpu->height,         mode,        gpu->body_count,
                        (uint32_t)x, (uint32_t)(x >> 32), (uint32_t)y, (uint32_t)(y >> 32)};
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), push);
    if (indirect) {
        VkDeviceSize offset =
            sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
            (VkDeviceSize)(dc_gpu_solver_word_offset(gpu) + SOLVER_DISPATCH_WORD) *
                sizeof(uint32_t);
        vkCmdDispatchIndirect(gpu->command, gpu->body_buffer, offset);
    } else
        vkCmdDispatch(gpu->command, 1, 1, 1);
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                         VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_HOST_READ_BIT};
    VkDependencyInfo dep = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .memoryBarrierCount = 1,
                            .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dep);
}
void dc_gpu_record_solver(dc_gpu_t *gpu) {
    dispatch(gpu, SOLVER_RESET, false);
    dispatch(gpu, SOLVER_BEGIN_FRAME, false);
    for (uint32_t step = 0; step < SOLVER_SUBSTEPS; ++step) {
        dispatch(gpu, SOLVER_PREDICT, false);
        dc_gpu_record_broadphase(gpu);
        dc_gpu_record_contacts(gpu);
        dispatch(gpu, SOLVER_PREPARE, false);
        dispatch(gpu, SOLVER_INIT_CONTACTS, true);
        dispatch(gpu, SOLVER_CLEAR, false);
        for (uint32_t iteration = 0; iteration < SOLVER_ITERATIONS; ++iteration) {
            dispatch(gpu, SOLVER_SOLVE, true);
            dispatch(gpu, SOLVER_APPLY, false);
        }
        dispatch(gpu, SOLVER_DERIVE_VELOCITY, false);
        dispatch(gpu, SOLVER_IMPULSE, true);
        dispatch(gpu, SOLVER_APPLY_VELOCITY, false);
    }
    dispatch(gpu, SOLVER_FINISH, false);
}
