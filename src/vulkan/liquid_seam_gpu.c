#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "dungeoncraft/liquid_seam_layout.h"
#include "gas_gpu_internal.h"
#include "renderer_internal.h"

enum { SEAM_BUFFER_COUNT = 11u };

typedef struct {
    dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE];
    uint32_t receipt;
} seam_plan_t;

typedef struct {
    uint32_t slot, x, y, width, height;
} seam_location_t;

typedef struct {
    seam_location_t first, second;
    uint32_t axis;
} seam_push_t;

struct dc_gpu_liquid_seam {
    fr_renderer_compute_t compute;
    VkBuffer plan;
    VkDeviceMemory memory;
    void *mapped;
    dc_gpu_t *pending_peer;
    bool pending_bulk;
};

_Static_assert(sizeof(dc_liquid_seam_flux_t) == 3u * sizeof(int32_t), "Liquid lane layout mismatch");
_Static_assert(sizeof(seam_plan_t) == (3u * DC_CHUNK_SIDE + 1u) * sizeof(uint32_t), "Liquid plan layout mismatch");
_Static_assert(sizeof(seam_push_t) == 11u * sizeof(uint32_t), "Liquid face push layout mismatch");

static dc_liquid_seam_result_t report(dc_liquid_seam_result_t result, char *err,
                                    uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return result;
}

static bool mapped_endpoint(dc_liquid_seam_endpoint_t endpoint) {
    const dc_gpu_t *gpu = endpoint.gpu;
    if (!gpu || !gpu->page_mapped || endpoint.slot >= dc_gpu_slot_capacity(gpu) ||
        endpoint.tile_x >= gpu->width / DC_CHUNK_SIDE || endpoint.tile_y >= gpu->height / DC_CHUNK_SIDE ||
        gpu->body_origin.x > INT64_MAX - (int64_t)endpoint.tile_x ||
        gpu->body_origin.y > INT64_MAX - (int64_t)endpoint.tile_y) return false;
    const uint32_t tile = endpoint.tile_y * gpu->page_width + endpoint.tile_x;
    const uint32_t *pages = gpu->page_mapped;
    return pages[tile] == endpoint.slot + 1u && gpu->slot_page[endpoint.slot] == tile &&
        endpoint.coord.x == gpu->body_origin.x + endpoint.tile_x &&
        endpoint.coord.y == gpu->body_origin.y + endpoint.tile_y;
}

bool dc_gpu_liquid_seam_mapped(dc_liquid_seam_endpoint_t first, dc_liquid_seam_endpoint_t second,
                               dc_liquid_seam_axis_t axis) {
    if (!mapped_endpoint(first) || !mapped_endpoint(second) || first.gpu == second.gpu ||
        !first.gpu->device_lifetime || first.gpu->device_lifetime != second.gpu->device_lifetime ||
        (first.gpu->thermal != NULL) != (second.gpu->thermal != NULL)) return false;
    dc_chunk_coord_t a = first.coord, b = second.coord;
    if (axis == DC_LIQUID_SEAM_X) return a.x != INT64_MAX && a.x + 1 == b.x && a.y == b.y;
    return axis == DC_LIQUID_SEAM_Y && a.y != INT64_MAX && a.y + 1 == b.y && a.x == b.x;
}

static bool valid_flux(int32_t units) {
    return units >= -(int32_t)DC_FLUID_FULL && units <= (int32_t)DC_FLUID_FULL;
}

static void release_resources(dc_gpu_t *gpu, struct dc_gpu_liquid_seam *seam) {
    if (!seam) return;
    fr_renderer_compute_destroy(gpu, &seam->compute);
    if (seam->mapped) vkUnmapMemory(gpu->device, seam->memory);
    if (seam->plan) vkDestroyBuffer(gpu->device, seam->plan, NULL);
    if (seam->memory) vkFreeMemory(gpu->device, seam->memory, NULL);
    free(seam);
}

void dc_gpu_liquid_seam_destroy(dc_gpu_t *gpu) {
    release_resources(gpu, gpu->liquid_seam);
    gpu->liquid_seam = NULL;
}

static struct dc_gpu_liquid_seam *prepare_resources(dc_gpu_t *first, const dc_gpu_t *second,
                                                   char *err, uint32_t cap) {
    struct dc_gpu_liquid_seam *seam = first->liquid_seam;
    if (!seam) {
        seam = calloc(1, sizeof(*seam));
        if (!seam || !dc_gpu_make_mapped_buffer(first, sizeof(seam_plan_t), &seam->plan,
            &seam->memory, &seam->mapped, err, cap)) {
            release_resources(first, seam);
            return NULL;
        }
    }

    const VkDescriptorBufferInfo buffers[SEAM_BUFFER_COUNT] = {
        {seam->plan, 0, sizeof(seam_plan_t)},
        {first->chunk_buffer, 0, VK_WHOLE_SIZE}, {second->chunk_buffer, 0, VK_WHOLE_SIZE},
        {first->bulk_phase_buffer, 0, VK_WHOLE_SIZE}, {second->bulk_phase_buffer, 0, VK_WHOLE_SIZE},
        {first->occupancy_buffer, 0, VK_WHOLE_SIZE}, {second->occupancy_buffer, 0, VK_WHOLE_SIZE},
        {first->water_color_buffer, 0, VK_WHOLE_SIZE}, {second->water_color_buffer, 0, VK_WHOLE_SIZE},
        {first->thermal_control_buffer, 0, VK_WHOLE_SIZE}, {second->thermal_control_buffer, 0, VK_WHOLE_SIZE}
    };
    if (!first->liquid_seam) {
        if (!fr_renderer_compute_init(first, &seam->compute, buffers, SEAM_BUFFER_COUNT,
            sizeof(seam_push_t), "build/shaders/liquid_seam.comp.spv", err, cap)) {
            release_resources(first, seam);
            return NULL;
        }
        first->liquid_seam = seam;
        return seam;
    }

    VkWriteDescriptorSet writes[SEAM_BUFFER_COUNT];
    for (uint32_t i = 0; i < SEAM_BUFFER_COUNT; ++i) {
        writes[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = seam->compute.descriptor, .dstBinding = i, .descriptorCount = 1u,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = buffers + i};
    }
    vkUpdateDescriptorSets(first->device, SEAM_BUFFER_COUNT, writes, 0, NULL);
    return seam;
}

static seam_location_t location(dc_liquid_seam_endpoint_t endpoint) {
    return (seam_location_t){endpoint.slot, endpoint.tile_x * DC_CHUNK_SIDE,
        endpoint.tile_y * DC_CHUNK_SIDE, endpoint.gpu->width, endpoint.gpu->height};
}

static void activate_liquid_work(dc_gpu_t *gpu, bool bulk) {
    gpu->fluid_snapshot_valid = false;
    gpu->fluid_work_possible = true;
    gpu->bulk_phase_work_possible |= bulk;
    fr_gas_gpu_geometry_changed(gpu, false);
}

static dc_liquid_seam_result_t complete_dispatch(dc_gpu_t *gpu, struct dc_gpu_liquid_seam *seam,
                                                 char *err, uint32_t cap) {
    const seam_plan_t *plan = seam->mapped;
    if (plan->receipt == DC_LIQUID_RECEIPT_REJECTED) {
        seam->pending_peer = NULL;
        return report(DC_LIQUID_SEAM_REJECTED, err, cap, "Liquid seam plan exceeds donor stock or face capacity");
    }
    if (plan->receipt != DC_LIQUID_RECEIPT_COMMITTED)
        return report(DC_LIQUID_SEAM_UNCONFIRMED, err, cap, "Liquid seam completion receipt is missing");
    activate_liquid_work(gpu, seam->pending_bulk);
    activate_liquid_work(seam->pending_peer, seam->pending_bulk);
    seam->pending_peer = NULL;
    return DC_LIQUID_SEAM_COMMITTED;
}

dc_liquid_seam_result_t dc_gpu_resolve_liquid_seam(dc_gpu_t *first, dc_gpu_t *second,
                                                  char *err, uint32_t cap) {
    struct dc_gpu_liquid_seam *seam = first ? first->liquid_seam : NULL;
    if (!seam || !second || seam->pending_peer != second || vkQueueWaitIdle(first->queue) != VK_SUCCESS)
        return report(DC_LIQUID_SEAM_UNCONFIRMED, err, cap, "Cannot reconcile the liquid seam dispatch");
    return complete_dispatch(first, seam, err, cap);
}

dc_liquid_seam_result_t dc_gpu_commit_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis,
    const dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE], char *err, uint32_t cap) {
    if (!flux || !dc_gpu_liquid_seam_mapped(first, second, axis))
        return report(DC_LIQUID_SEAM_REJECTED, err, cap, "Liquid seam endpoints do not share a mapped face");
    dc_gpu_t *gpu = first.gpu;
    if (gpu->liquid_seam && gpu->liquid_seam->pending_peer)
        return report(DC_LIQUID_SEAM_UNCONFIRMED, err, cap, "A liquid seam receipt is still pending");
    bool water = false, bulk = false;
    for (uint32_t i = 0; i < DC_CHUNK_SIDE; ++i) {
        if (!valid_flux(flux[i].water) || !valid_flux(flux[i].oil) || !valid_flux(flux[i].slime))
            return report(DC_LIQUID_SEAM_REJECTED, err, cap, "Liquid seam flux exceeds one cell volume");
        water |= flux[i].water != 0;
        bulk |= flux[i].oil != 0 || flux[i].slime != 0;
    }
    if (!water && !bulk) return DC_LIQUID_SEAM_COMMITTED;
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return report(DC_LIQUID_SEAM_UNCONFIRMED, err, cap, "Cannot synchronize liquid seam endpoints");
    if (bulk && (!dc_gpu_ensure_bulk_phase_pipeline(gpu, err, cap) ||
                 !dc_gpu_ensure_bulk_phase_pipeline(second.gpu, err, cap)))
        return DC_LIQUID_SEAM_REJECTED;
    struct dc_gpu_liquid_seam *seam = prepare_resources(gpu, second.gpu, err, cap);
    if (!seam || !fr_renderer_begin(gpu))
        return report(DC_LIQUID_SEAM_REJECTED, err, cap, "Cannot prepare the liquid seam dispatch");
    seam_plan_t *plan = seam->mapped;
    memcpy(plan->flux, flux, sizeof(plan->flux));
    plan->receipt = 0u;
    const seam_push_t push = {location(first), location(second), axis};
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, seam->compute.pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, seam->compute.pipeline_layout,
        0, 1, &seam->compute.descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, seam->compute.pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(gpu->command, 1, 1, 1);
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT);
    seam->pending_peer = second.gpu;
    seam->pending_bulk = bulk;
    if (!fr_renderer_submit(gpu))
        return report(DC_LIQUID_SEAM_UNCONFIRMED, err, cap, "Liquid seam submission was not confirmed");
    return complete_dispatch(gpu, seam, err, cap);
}

bool dc_gpu_project_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE],
    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE], char *err, uint32_t cap) {
    (void)first; (void)second; (void)axis; (void)seconds;
    (void)previous_normal; (void)samples; (void)err; (void)cap;
    return false;
}
