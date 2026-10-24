#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_internal.h"
#include "runtime_body_gpu.h"

enum { SOLVER_ITERATIONS = 8, SOLVER_SUBSTEPS = 8, SOLVER_DISPATCH_WORD = 6,
       SOLVER_GRAIN_DISPATCH_WORD = 13, SOLVER_PHASE_MASK = 255 };
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
    SOLVER_FINISH,
    SOLVER_BOND_POSITION,
    SOLVER_BOND_VELOCITY,
    SOLVER_BOND_RESET,
    SOLVER_GRAIN_PREPARE,
    SOLVER_GRAIN_POSITION,
    SOLVER_GRAIN_VELOCITY,
    SOLVER_GRAIN_BED_INIT,
    SOLVER_GRAIN_BED_JUMP,
    SOLVER_GRAIN_BED_PLAN,
    SOLVER_GRAIN_BED_PUBLISH
} solver_phase_t;
_Static_assert(SOLVER_GRAIN_BED_PUBLISH + 1 == DC_GPU_SOLVER_PHASE_COUNT,
               "GPU solver phase count changed");
static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap)
        snprintf(err, cap, "%s", message);
    return false;
}
static dc_gpu_body_record_t *find_body(dc_gpu_t *gpu, uint32_t id) {
    if (!gpu || !id)
        return NULL;
    dc_gpu_refresh_body_registry(gpu);
    for (uint32_t i = 0; i < gpu->body_count; ++i)
        if (gpu->body_ids[i] == id)
            return &(*dc_gpu_body_record(gpu,i));
    return NULL;
}
bool dc_gpu_set_rigid_solver(dc_gpu_t *gpu, bool enabled) {
    if (!gpu)
        return false;
    if (!enabled && gpu->structural_enabled)
        return false;
    gpu->rigid_solver_enabled = enabled;
    uint32_t *data = dc_gpu_solver_data(gpu);
    data[0] = enabled;
    data[1] = SOLVER_ITERATIONS;
    data[2] = SOLVER_SUBSTEPS;
    return true;
}
bool dc_gpu_set_body_motion(dc_gpu_t *gpu, uint32_t id, float angle,
                            float omega, uint32_t flags, char *err,
                            uint32_t cap) {
    if (!isfinite(angle) || !isfinite(omega) || fabsf(omega) > .5f ||
        (flags & ~3u) || fabsf(angle) > 1000000.f)
        return error(err, cap, "Invalid rigid motion");
    dc_gpu_body_record_t *b = find_body(gpu, id);
    if (!b)
        return error(err, cap, "GPU body ID not found");
    if (b->reserved[0] == DC_GPU_STRUCTURAL_TAG)
        return error(err, cap, "Structural motion is owned by its graph");
    const float pi = 3.14159265358979323846f, turn = 2.f * pi;
    angle -= (float)(int)(angle / turn) * turn;
    if (angle > pi)
        angle -= turn;
    if (angle < -pi)
        angle += turn;
    b->angle = angle;
    b->angular_velocity = omega;
    b->motion_flags = flags;
    if (!(b->piece_count & DC_GPU_PIXEL_BODY_FLAG))
        b->motion_ready = 0;
    b->rest_ticks = 0;
    gpu->body_refresh_pending = true;
    return true;
}
bool dc_gpu_read_body_motion(dc_gpu_t *gpu, uint32_t id,
                             dc_gpu_body_motion_t *motion, char *err,
                             uint32_t cap) {
    dc_gpu_body_record_t *b = find_body(gpu, id);
    if (!b || !motion)
        return error(err, cap, "Invalid rigid motion readback");
    *motion = (dc_gpu_body_motion_t){
        b->angle,   b->angular_velocity, b->mass,      b->inertia,
        b->density, b->center[0],        b->center[1], b->motion_flags};
    return true;
}
bool dc_gpu_read_rigid_solver_stats(dc_gpu_t *gpu,
                                    dc_gpu_rigid_solver_stats_t *stats) {
    if (!gpu || !stats)
        return false;
    memcpy(stats, dc_gpu_solver_data(gpu), sizeof(*stats));
    return true;
}
static bool pixel_hydro_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/pixel_hydro.comp.spv",
                                   &module, err, cap))
        return false;
    VkSpecializationMapEntry entry = {.constantID = 0,
                                      .size = sizeof(uint32_t)};
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main"},
        .layout = gpu->pipeline_layout};
    VkResult result = VK_SUCCESS;
    for (uint32_t phase = 0; phase < 5 && result == VK_SUCCESS; ++phase) {
        VkSpecializationInfo specialization = {.mapEntryCount = 1,
                                               .pMapEntries = &entry,
                                               .dataSize = sizeof(phase),
                                               .pData = &phase};
        info.stage.pSpecializationInfo = &specialization;
        result =
            vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &info,
                                     NULL, &gpu->pixel_hydro_pipelines[phase]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS ||
           error(err, cap, "Cannot create pixel hydro pipelines");
}

bool dc_gpu_solver_pipeline_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkShaderModule module = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, "build/shaders/rigid_solver.comp.spv",
                                   &module, err, cap))
        return false;
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main"},
        .layout = gpu->pipeline_layout};
    VkResult result = VK_SUCCESS;
    const char *grouped=getenv("DC_SOLVER_GROUPED_REDUCTIONS");
    const char *zero=getenv("DC_SOLVER_SKIP_ZERO_REDUCTIONS");
    const char *fused=getenv("DC_SOLVER_FUSED_GRAIN");
    const char *overlap=getenv("DC_SOLVER_APPLY_OVERLAP");
    gpu->solver_fused_grain=fused&&strcmp(fused,"1")==0;
    gpu->solver_apply_overlap=overlap&&strcmp(overlap,"1")==0;
    VkSpecializationMapEntry entries[]={{0u,0u,4u},{5u,4u,4u},{4u,8u,4u},{6u,12u,4u}};
    for (uint32_t mode = 0;
         mode < DC_GPU_SOLVER_PHASE_COUNT && result == VK_SUCCESS; ++mode) {
        uint32_t values[]={mode,grouped&&strcmp(grouped,"1")==0,zero&&strcmp(zero,"1")==0,gpu->solver_fused_grain};
        VkSpecializationInfo phase = {.mapEntryCount = 4,
                                      .pMapEntries = entries,
                                      .dataSize = sizeof(values),
                                      .pData = values};
        info.stage.pSpecializationInfo = &phase;
        result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &gpu->solver_pipelines[mode]);
    }
    vkDestroyShaderModule(gpu->device, module, NULL);
    return result == VK_SUCCESS
               ? pixel_hydro_pipeline_init(gpu, err, cap)
               : error(err, cap, "Cannot create GPU rigid solver pipeline");
}
static void record_pixel_hydro(dc_gpu_t *gpu) {
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width,  gpu->height,
                        0,           dc_gpu_runtime_body_count(gpu),
                        (uint32_t)x, (uint32_t)(x >> 32),
                        (uint32_t)y, (uint32_t)(y >> 32)};
    const uint32_t phases[] = {0, 3, 1, 2, 4};
    for (unsigned stage = 0; stage < sizeof(phases) / sizeof(*phases);
         ++stage) {
        uint32_t phase = phases[stage];
        push[2] = phase;
        vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          gpu->pixel_hydro_pipelines[phase]);
        vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
        if (phase == 1 || phase == 2)
            vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                                  (phase == 1 ? 33u : 36u) * sizeof(uint32_t));
        else {
            uint32_t groups=phase==3?(gpu->runtime_body_consumers?(dc_gpu_body_capacity(gpu)+63u)/64u:1u):dc_gpu_body_capacity(gpu);
            uint32_t x=groups<65535u?groups:65535u;
            vkCmdDispatch(gpu->command,x,(groups+x-1u)/x,1);
        }
        if(phase==3u&&gpu->runtime_body_consumers) {
            VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
            vkCmdPipelineBarrier2(gpu->command,&dependency);
            dc_gpu_runtime_record_prefix(gpu,dc_gpu_body_capacity(gpu),UINT32_MAX);
            vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,gpu->pixel_hydro_pipelines[phase]);
            push[2]=phase|(1u<<8u);
            vkCmdPushConstants(gpu->command,gpu->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
            vkCmdDispatch(gpu->command,(dc_gpu_body_capacity(gpu)+63u)/64u,1,1);
        }
        VkMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                             VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                             VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT};
        VkDependencyInfo dependency = {.sType =
                                           VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                       .memoryBarrierCount = 1,
                                       .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(gpu->command, &dependency);
    }
}
static void dispatch(dc_gpu_t *gpu, uint32_t mode, bool indirect) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->solver_pipelines[mode & SOLVER_PHASE_MASK]);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width,  gpu->height,
                        mode,        dc_gpu_runtime_body_count(gpu),
                        (uint32_t)x, (uint32_t)(x >> 32),
                        (uint32_t)y, (uint32_t)(y >> 32)};
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    uint32_t phase = mode & SOLVER_PHASE_MASK;
    if (phase == SOLVER_GRAIN_BED_PLAN || phase == SOLVER_GRAIN_BED_PUBLISH) {
        vkCmdDispatch(gpu->command, 1, 1, 1);
    } else if (phase >= SOLVER_GRAIN_BED_INIT) {
        vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
            DC_GRAIN_BED_WORK_WORD * sizeof(uint32_t));
    } else if (phase == SOLVER_BEGIN_FRAME || phase == SOLVER_PREPARE || phase == SOLVER_FINISH) {
        vkCmdDispatch(gpu->command,(dc_gpu_runtime_body_count(gpu)+63u)/64u,1,1);
    } else if (gpu->solver_fused_grain&&(phase==SOLVER_APPLY||phase==SOLVER_APPLY_VELOCITY)) {
        vkCmdDispatchIndirect(gpu->command,gpu->mpm_activity_buffer,DC_RIGID_APPLY_WORK_WORD*sizeof(uint32_t));
    } else if (phase == SOLVER_PREDICT || phase == SOLVER_CLEAR ||
               phase == SOLVER_APPLY || phase == SOLVER_DERIVE_VELOCITY ||
               phase == SOLVER_APPLY_VELOCITY) {
        vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
            DC_RIGID_BODY_WORK_WORD * sizeof(uint32_t));
    } else if (phase >= SOLVER_BOND_POSITION && phase <= SOLVER_BOND_RESET) {
        VkDeviceSize offset =
            sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
            (VkDeviceSize)(dc_gpu_structural_word_offset(gpu) + 9u) *
                sizeof(uint32_t);
        vkCmdDispatchIndirect(gpu->command, gpu->body_buffer, offset);
    } else if (indirect) {
        VkDeviceSize offset =
            sizeof(dc_gpu_body_record_t) * DC_GPU_BODY_CAPACITY +
            (VkDeviceSize)(dc_gpu_solver_word_offset(gpu) +
                           (phase >= SOLVER_GRAIN_PREPARE ? SOLVER_GRAIN_DISPATCH_WORD :
                            SOLVER_DISPATCH_WORD)) *
                sizeof(uint32_t);
        vkCmdDispatchIndirect(gpu->command, gpu->body_buffer, offset);
    } else
        vkCmdDispatch(gpu->command, 1, 1, 1);
    if(gpu->runtime_body_consumers&&gpu->solver_apply_overlap&&!gpu->solver_fused_grain&&
       (phase==SOLVER_APPLY||phase==SOLVER_APPLY_VELOCITY)) {
        const int64_t margin=INT32_MAX/DC_FLUID_FULL/DC_CHUNK_SIDE+1;
        /* Grain anchors were validated by INIT_CONTACTS and stay immutable.
         * This margin also rules out publish() writing a world-overflow fault.
         * Both apply passes can therefore finish at the following barrier. */
        if(gpu->body_origin.x>=INT64_MIN+margin&&gpu->body_origin.x<=INT64_MAX-margin&&
           gpu->body_origin.y>=INT64_MIN+margin&&gpu->body_origin.y<=INT64_MAX-margin) return;
    }
    bool publish_indirect=!gpu->runtime_body_consumers||phase==SOLVER_PREPARE||
        phase==SOLVER_GRAIN_BED_PLAN||phase==SOLVER_GRAIN_BED_PUBLISH;
    VkMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                        (publish_indirect ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : 0) |
                        (phase == SOLVER_FINISH ? VK_PIPELINE_STAGE_2_HOST_BIT : 0),
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                         VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                         (publish_indirect ? VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT : 0) |
                         (phase == SOLVER_FINISH ? VK_ACCESS_2_HOST_READ_BIT : 0)};
    VkDependencyInfo dep = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .memoryBarrierCount = 1,
                            .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dep);
}
void dc_gpu_record_solver_uncached(dc_gpu_t *gpu) {
    gpu->solver_recording = true;
    dispatch(gpu, SOLVER_GRAIN_BED_PLAN | (DC_GPU_CHUNK_SLOTS << 8u), false);
    dispatch(gpu, SOLVER_GRAIN_BED_INIT, false);
    dispatch(gpu, SOLVER_GRAIN_BED_PUBLISH, false);
    for (uint32_t span=1, pass=0; span<gpu->height; span*=2, pass++)
        dispatch(gpu, SOLVER_GRAIN_BED_JUMP | (pass << 8u), false);
    dispatch(gpu, SOLVER_RESET, false);
    dispatch(gpu, SOLVER_BEGIN_FRAME, false);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             gpu->timestamp_pool, 16);
    record_pixel_hydro(gpu);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                             gpu->timestamp_pool, 17);
    for (uint32_t step = 0; step < SOLVER_SUBSTEPS; ++step) {
        dispatch(gpu, SOLVER_PREDICT, false);
        if (gpu->structural_enabled)
            dispatch(gpu, SOLVER_BOND_RESET, false);
        dc_gpu_record_broadphase(gpu);
        if (gpu->component_profile)
            vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                 gpu->timestamp_pool, 18 + step * 2);
        dc_gpu_record_contacts(gpu);
        if (gpu->component_profile)
            vkCmdWriteTimestamp2(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                 gpu->timestamp_pool, 19 + step * 2);
        dispatch(gpu, SOLVER_PREPARE, false);
        dispatch(gpu, SOLVER_GRAIN_PREPARE, true);
        dispatch(gpu, SOLVER_INIT_CONTACTS, true);
        dispatch(gpu, SOLVER_CLEAR, false);
        for (uint32_t iteration = 0; iteration < SOLVER_ITERATIONS;
             ++iteration) {
            dispatch(gpu, SOLVER_SOLVE, true);
            dispatch(gpu, SOLVER_APPLY | ((iteration + 1u) << 8u), false);
            if(!gpu->solver_fused_grain) dispatch(gpu, SOLVER_GRAIN_POSITION | ((iteration + 1u) << 8u), true);
            if (gpu->structural_enabled)
                dispatch(gpu, SOLVER_BOND_POSITION, false);
        }
        dispatch(gpu, SOLVER_DERIVE_VELOCITY, false);
        dispatch(gpu, SOLVER_IMPULSE, true);
        dispatch(gpu, SOLVER_APPLY_VELOCITY | ((SOLVER_ITERATIONS + 1u) << 8u), false);
        if(!gpu->solver_fused_grain) dispatch(gpu, SOLVER_GRAIN_VELOCITY | ((SOLVER_ITERATIONS + 1u) << 8u), true);
        if (gpu->structural_enabled)
            dispatch(gpu, SOLVER_BOND_VELOCITY, false);
    }
    dispatch(gpu, SOLVER_FINISH, false);
    gpu->solver_recording = false;
}
