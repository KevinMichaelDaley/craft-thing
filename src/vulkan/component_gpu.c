#include "gpu_internal.h"

bool dc_gpu_read_extraction_stats(dc_gpu_t *gpu,dc_gpu_extraction_stats_t *stats) {
    if(!gpu||!stats||!gpu->body_mapped||vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS) return false;
    *stats=(dc_gpu_extraction_stats_t){.body_capacity=DC_GPU_BODY_CAPACITY};
    const dc_gpu_body_record_t *bodies=gpu->body_mapped;
    for(uint32_t slot=0;slot<DC_GPU_BODY_CAPACITY;slot++)
        stats->owned_bodies+=bodies[slot].body.active!=0u||bodies[slot].reserved[0]!=0u;
    stats->free_body_slots=stats->body_capacity-stats->owned_bodies;
    if(!gpu->automatic_components_enabled) return true;
    const uint32_t *base=dc_gpu_structural_data(gpu),*graph=base+dc_gpu_structural_graph_offset();
    stats->arena_used_words=graph[40001];stats->arena_capacity_words=graph[40002];
    stats->pending=base[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]!=0u;
    stats->selected_components=graph[36257]<64u?graph[36257]:64u;
    uint32_t free_words=stats->arena_capacity_words>stats->arena_used_words?
        stats->arena_capacity_words-stats->arena_used_words:0u;
    for(uint32_t candidate=0;candidate<stats->selected_components;candidate++) {
        const uint32_t *record=graph+36300u+candidate*24u;
        if(record[14]!=0u) {stats->committed_components++;continue;}
        if(record[6]!=0u) {stats->unknown_components++;continue;}
        if(record[7]!=0u) {
            stats->no_body_slot_components+=stats->free_body_slots==0u;
            stats->no_arena_components+=record[10]>free_words;
        }
    }
    return true;
}

bool dc_gpu_enable_automatic_components(dc_gpu_t *gpu) {
    if (!gpu)
        return false;
    gpu->automatic_components_enabled = true;
    gpu->structural_enabled = true;
    dc_gpu_structural_data(gpu)[19] = 1;
    dc_gpu_structural_data(gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = 1;
    return dc_gpu_set_rigid_solver(gpu, true);
}

static void extraction_stage(dc_gpu_t *gpu, uint32_t push[8], uint32_t mode,
                             uint32_t stride, uint32_t groups) {
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      gpu->component_extract_pipelines[mode]);
    push[2] = mode;
    push[3] = stride;
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, 8 * sizeof(uint32_t),
                       push);
    if (mode == 26)
        vkCmdDispatch(gpu->command, 1, 1, 1);
    else {
        uint32_t offset = mode == 1 || mode == 2 || mode == 4 || mode == 31
                              ? 24u
                          : mode == 9                ? 15u
                          : mode == 10 || mode == 12 ? 21u
                          : mode == 3 || mode == 30  ? (stride ? 36u : 30u)
                          : mode == 13               ? 12u
                          : mode == 19 || mode == 32 || mode == 34 ? 27u
                          : groups == 1                            ? 3u
                          : groups == 64                           ? 6u
                                                                   : 0u;
        vkCmdDispatchIndirect(gpu->command, gpu->mpm_activity_buffer,
                              offset * sizeof(uint32_t));
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
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .memoryBarrierCount = 1,
                                   .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}

void dc_gpu_record_component_baseline(dc_gpu_t *gpu) {
    if (!gpu->component_baseline_pending) return;
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout,0,1,&gpu->descriptor,0,NULL);
    uint32_t push[8]={gpu->width,gpu->height,0};
    uint32_t groups=(DC_GPU_CHUNK_SLOTS*4096u+127u)/128u;
    extraction_stage(gpu,push,26,0,1);
    extraction_stage(gpu,push,9,0,groups);
    extraction_stage(gpu,push,11,0,groups);
    extraction_stage(gpu,push,20,0,groups);
    gpu->component_baseline_pending=false;
}

void dc_gpu_record_component_extraction(dc_gpu_t *gpu) {
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0,
                            NULL);
    uint64_t x = (uint64_t)gpu->body_origin.x, y = (uint64_t)gpu->body_origin.y;
    uint32_t push[8] = {gpu->width,  gpu->height,        0,
                        0,           (uint32_t)x,        (uint32_t)(x >> 32),
                        (uint32_t)y, (uint32_t)(y >> 32)};
    uint32_t cells = DC_GPU_CHUNK_SLOTS * 4096u, groups = (cells + 127u) / 128u;
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 4);
    extraction_stage(gpu, push, 26, 0, 1);
    extraction_stage(gpu, push, 9, 0, (DC_GPU_CHUNK_SLOTS * 4u + 127u) / 128u);
    extraction_stage(gpu, push, 10, 0, groups);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 5);
    extraction_stage(gpu, push, 12, 0, groups);
    extraction_stage(gpu, push, 13, 0, groups);
    extraction_stage(gpu, push, 14, 0, groups);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 6);
    extraction_stage(gpu, push, 1, cells / 32u, groups);
    extraction_stage(gpu, push, 2, cells / 32u, groups);
    extraction_stage(gpu, push, 3, cells / 32u, groups);
    extraction_stage(gpu, push, 29, cells / 32u, 1);
    extraction_stage(gpu, push, 30, cells / 32u, groups);
    extraction_stage(gpu, push, 31, cells / 32u, groups);
    extraction_stage(gpu, push, 4, 0, groups);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 7);
    extraction_stage(gpu, push, 5, 0, groups);
    extraction_stage(gpu, push, 6, 0, 1);
    extraction_stage(gpu, push, 7, 0, groups);
    extraction_stage(gpu, push, 8, 0, 1);
    extraction_stage(gpu, push, 34, 0, groups);
    extraction_stage(gpu, push, 15, 0, groups);
    extraction_stage(gpu, push, 19, 0, groups);
    extraction_stage(gpu, push, 3, 0, groups);
    extraction_stage(gpu, push, 29, 0, 1);
    extraction_stage(gpu, push, 30, 0, groups);
    extraction_stage(gpu, push, 32, 0, groups);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 8);
    extraction_stage(gpu, push, 16, 0, 1);
    extraction_stage(gpu, push, 17, 0, 64);
    extraction_stage(gpu, push, 18, 0, 1);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 9);
    extraction_stage(gpu, push, 21, 0, 1);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 10);
    extraction_stage(gpu, push, 22, 0, 64);
    extraction_stage(gpu, push, 28, 0, groups);
    extraction_stage(gpu, push, 23, 0, 64);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 11);
    extraction_stage(gpu, push, 11, 0, groups);
    extraction_stage(gpu, push, 24, 0, groups);
    extraction_stage(gpu, push, 25, 0, 1);
    if (gpu->component_profile)
        vkCmdWriteTimestamp2(gpu->command,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             gpu->timestamp_pool, 12);
}
