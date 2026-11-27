#include <stdio.h>
#include <string.h>

#include "../vulkan/revolute_gpu_internal.h"
#include "../vulkan/runtime_body_gpu.h"
#include "offscreen_internal.h"

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

bool dc_offscreen_body_island(dc_gpu_t *gpu, dc_chunk_coord_t coord) {
    if (!gpu || !gpu->body_count || gpu->thermal || gpu->structural_graph_possible || fr_gpu_revolute_span(gpu))
        return false;
    bool found = false;
    for (uint32_t i = 0; i < gpu->body_count; ++i) {
        const dc_gpu_body_record_t *record = dc_gpu_body_record(gpu, i);
        if (!record->body.id) continue;
        const dc_body_storage_owner_t *owner = dc_gpu_body_owner(gpu, i);
        if (!record->body.active || record->reserved[0] || (record->piece_count & DC_GPU_PIXEL_BODY_FLAG) ||
            owner->source_transaction.low || owner->source_transaction.high ||
            (record->motion_flags & (DC_BODY_MOTION_REACTIVE_MASS | DC_BODY_MOTION_THERMAL_GRID))) return false;
        uint64_t x = record->chunk_x[0] | ((uint64_t)record->chunk_x[1] << 32);
        uint64_t y = record->chunk_y[0] | ((uint64_t)record->chunk_y[1] << 32);
        if (x != (uint64_t)coord.x || y != (uint64_t)coord.y) return false;
        /* The sum of the two side lengths bounds the rotated hull about its anchor. */
        int32_t radius = (int32_t)(record->body.width + record->body.height) * DC_FLUID_FULL;
        int32_t extent = DC_CHUNK_SIDE * DC_FLUID_FULL;
        if (record->local_x_fp < radius || record->local_y_fp < radius ||
            record->local_x_fp > extent-radius || record->local_y_fp > extent-radius) return false;
        found = true;
    }
    return found;
}

static bool free_slot(const dc_gpu_t *gpu, uint32_t i) {
    const dc_body_storage_owner_t *owner = dc_gpu_body_owner(gpu, i);
    return !gpu->body_ids[i] && owner->generation != UINT32_MAX &&
        owner->state != DC_BODY_OWNER_RETIRED && owner->state != DC_BODY_OWNER_RESERVED;
}

bool dc_offscreen_move_bodies(dc_gpu_t *from, dc_gpu_t *to, dc_chunk_coord_t coord, char *err, uint32_t cap) {
    if (!from || !to || from == to || from->device_lifetime != to->device_lifetime ||
        from->runtime_body_consumers != to->runtime_body_consumers || vkQueueWaitIdle(from->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot synchronize rigid handoff");
    dc_gpu_refresh_body_registry(from);
    dc_gpu_refresh_body_registry(to);
    if (!dc_offscreen_body_island(from, coord))
        return error(err, cap, "Rigid handoff requires a complete independent group");
    uint32_t needed = 0, available = 0, capacity = dc_gpu_body_capacity(to);
    for (uint32_t i = 0; i < from->body_count; ++i) {
        if (!from->body_ids[i]) continue;
        if (dc_gpu_find_body_record(to, from->body_ids[i])) return error(err, cap, "Rigid handoff ID already exists");
        ++needed;
    }
    for (uint32_t i = 0; i < capacity; ++i) available += free_slot(to, i);
    if (available < needed && (needed-available > UINT32_MAX-capacity ||
        !dc_gpu_runtime_bodies_reserve(to, capacity + (needed-available), err, cap))) return false;

    uint32_t next = 0;
    for (uint32_t i = 0; i < from->body_count; ++i) {
        if (!from->body_ids[i]) continue;
        while (!free_slot(to, next)) ++next;
        dc_gpu_body_record_t *source = dc_gpu_body_record(from, i);
        *dc_gpu_body_record(to, next) = *source;
        size_t solver_bytes = (from->runtime_body_consumers ? 64u : DC_GPU_SOLVER_BODY_WORDS) * sizeof(uint32_t);
        memcpy(dc_gpu_body_solver_state(to, next), dc_gpu_body_solver_state(from, i), solver_bytes);
        memcpy(dc_gpu_body_hydro_state(to, next), dc_gpu_body_hydro_state(from, i), 128u * sizeof(uint32_t));
        to->body_ids[next++] = source->body.id;
        memset(source, 0, sizeof(*source));
        memset(dc_gpu_body_solver_state(from, i), 0, solver_bytes);
        memset(dc_gpu_body_hydro_state(from, i), 0, 128u * sizeof(uint32_t));
        dc_body_storage_owner_t *owner = dc_gpu_body_owner(from, i);
        *owner = (dc_body_storage_owner_t){.generation = owner->generation,
            .state = owner->generation == UINT32_MAX ? DC_BODY_OWNER_RETIRED : DC_BODY_OWNER_FREE};
        from->body_ids[i] = 0;
    }
    dc_gpu_refresh_body_registry(from);
    dc_gpu_refresh_body_registry(to);
    from->body_refresh_pending = to->body_refresh_pending = true;
    return true;
}
