#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "../vulkan/gpu_internal.h"
#include "level_internal.h"
#include "offscreen_internal.h"

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

bool dc_level_view_set_marker_correction(dc_level_view_t *view, bool enabled) {
    if (!view || !view->gpu) return false;
    for (const dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        dc_gpu_t *gpu = p->workspace->gpu;
        if (gpu->marker_correction != enabled && dc_gpu_liquid_owner_held(gpu)) return false;
    }
    if (!dc_gpu_set_marker_correction(view->gpu, enabled)) return false;
    for (const dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next)
        dc_gpu_set_marker_correction(p->workspace->gpu, enabled);
    return true;
}

bool dc_level_view_set_fluid_interval(dc_level_view_t *view, uint32_t interval) {
    if (!view || !view->gpu) return false;
    for (const dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        if (dc_gpu_liquid_owner_held(p->workspace->gpu)) return false;
    }
    if (!dc_gpu_set_fluid_interval(view->gpu, interval)) return false;
    for (const dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next)
        dc_gpu_set_fluid_interval(p->workspace->gpu, interval);
    return true;
}

bool dc_level_view_stage_world_save(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view || !view->save || !view->stream || view->closing || view->liquid_seam_failed)
        return error(err, cap, "Cannot stage this world save");
    if (view->world_save_staged) return true;
    if (!dc_world_save_begin(view->save, err, cap)) return false;
    const char *staging = dc_world_save_directory(view->save, true);
    char body_path[sizeof(view->body_path)];
    int length = snprintf(body_path, sizeof(body_path), "%s/" DC_WORLD_BODY_RECORD, staging);
    if (length < 0 || (size_t)length >= sizeof(body_path) || !dc_stream_stage(view->stream, staging)) {
        if (!dc_world_save_abort(view->save)) view->liquid_seam_failed = true;
        return error(err, cap, "Cannot attach the stream to the staged world save");
    }
    memcpy(view->body_path, body_path, (size_t)length + 1u);
    view->world_save_staged = true;
    return true;
}

static bool shared_face(dc_chunk_coord_t resident, dc_chunk_coord_t offscreen,
    dc_liquid_seam_axis_t *axis, bool *resident_first) {
    if (resident.y == offscreen.y) {
        *axis = DC_LIQUID_SEAM_X;
        if (resident.x < offscreen.x && resident.x + 1 == offscreen.x) {
            *resident_first = true;
            return true;
        }
        *resident_first = false;
        return offscreen.x < resident.x && offscreen.x + 1 == resident.x;
    }
    if (resident.x != offscreen.x) return false;
    *axis = DC_LIQUID_SEAM_Y;
    if (resident.y < offscreen.y && resident.y + 1 == offscreen.y) {
        *resident_first = true;
        return true;
    }
    *resident_first = false;
    return offscreen.y < resident.y && offscreen.y + 1 == resident.y;
}

static bool aligned_interval(const dc_level_view_t *view, const dc_chunk_slot_t *resident,
    const dc_level_offscreen_handoff_t *handoff, const dc_level_liquid_exchange_t *exchange,
    uint32_t face) {
    const dc_offscreen_t *workspace = handoff->workspace;
    return isfinite(exchange->begin_seconds) && isfinite(exchange->end_seconds) &&
        exchange->begin_seconds >= 0. && exchange->begin_seconds < exchange->end_seconds &&
        exchange->end_seconds == view->world_seconds && resident->time.known && workspace->time.known &&
        exchange->end_seconds == resident->time.seconds && exchange->end_seconds == workspace->time.seconds &&
        resident->admitted_time.known && handoff->source_time.known &&
        exchange->begin_seconds >= resident->admitted_time.seconds &&
        exchange->begin_seconds >= handoff->source_time.seconds &&
        exchange->begin_seconds >= handoff->liquid_face_end[face];
}

static bool touches_resident(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_liquid_seam_axis_t axis;
        bool first;
        const dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state == DC_SLOT_ACTIVE && shared_face(slot->coord, coord, &axis, &first)) return true;
    }
    return false;
}

dc_level_offscreen_result_t dc_level_offscreen_tick_pair(dc_level_view_t *view, float seconds,
    char *err, uint32_t cap) {
    dc_offscreen_t *workspace = NULL;
    for (dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        if (p->disk_source || !p->admitted || !p->workspace->time.known ||
            p->workspace->time.seconds != view->world_seconds || !touches_resident(view, p->coord)) continue;
        if (workspace && workspace != p->workspace) {
            error(err, cap, "A water basin spans multiple offscreen workspaces");
            return DC_LEVEL_OFFSCREEN_FAILED;
        }
        workspace = p->workspace;
    }
    if (!workspace) return DC_LEVEL_OFFSCREEN_UNMATCHED;
    dc_liquid_seam_endpoint_t tiles[DC_GPU_CHUNK_SLOTS + OFFSCREEN_SLOTS];
    uint32_t count = 0;
    if (workspace->failed || workspace->held_writes || view->closing || view->liquid_seam_failed ||
        dc_gpu_liquid_seam_pending(view->gpu) || dc_gpu_liquid_seam_pending(workspace->gpu)) goto stale;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        const dc_chunk_slot_t *s = &view->table.slots[i];
        if (s->state != DC_SLOT_ACTIVE) continue;
        if (i >= dc_gpu_slot_capacity(view->gpu) || !s->time.known || s->time.seconds != view->world_seconds ||
            !s->generation || s->pinned != (view->water_generations[i] != 0u) ||
            (s->pinned && s->generation != view->water_generations[i])) goto stale;
        uint32_t page = view->gpu->slot_page[i];
        if (page == UINT32_MAX || !view->gpu->page_width) goto stale;
        tiles[count++] = (dc_liquid_seam_endpoint_t){.gpu = view->gpu, .coord = s->coord, .slot = i,
            .tile_x = page % view->gpu->page_width, .tile_y = page / view->gpu->page_width};
    }
    uint32_t residents = count;
    for (dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        if (p->workspace != workspace) continue;
        uint32_t slot;
        if (!p->admitted || p->write_generation || !p->generation ||
            !dc_offscreen_page_at(workspace, p->coord, &slot) || workspace->generations[slot] != p->generation ||
            count >= residents + OFFSCREEN_SLOTS) goto stale;
        tiles[count++] = (dc_liquid_seam_endpoint_t){.gpu = workspace->gpu, .coord = p->coord, .slot = slot,
            .tile_x = slot % OFFSCREEN_SIDE, .tile_y = slot / OFFSCREEN_SIDE};
    }
    if (!dc_level_view_stage_world_save(view, err, cap)) return DC_LEVEL_OFFSCREEN_FAILED;
    dc_liquid_seam_result_t result = dc_gpu_tick_water_cohort(tiles, count, seconds, err, cap);
    if (result != DC_LIQUID_SEAM_COMMITTED) {
        if (result == DC_LIQUID_SEAM_UNCONFIRMED) view->liquid_seam_failed = workspace->failed = true;
        return DC_LEVEL_OFFSCREEN_FAILED;
    }
    workspace->time.seconds = view->world_seconds + seconds;
    for (uint32_t i = 0; i < residents; ++i) {
        const uint32_t slot = tiles[i].slot;
        dc_chunk_slot_t *s = &view->table.slots[slot];
        s->pinned = dc_gpu_liquid_owner_held(view->gpu);
        view->water_generations[slot] = s->pinned ? s->generation : 0;
    }
    for (dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        if (p->workspace != workspace) continue;
        p->liquid_paired = true;
        for (unsigned f = 0; f < 4u; ++f) p->liquid_face_end[f] = workspace->time.seconds;
    }
    return DC_LEVEL_OFFSCREEN_HANDLED;

stale:
    error(err, cap, "Water tick requires current generations and a common interval start");
    return DC_LEVEL_OFFSCREEN_FAILED;
}

dc_liquid_seam_result_t dc_level_offscreen_exchange_liquids(dc_level_view_t *view,
    const dc_level_liquid_exchange_t *exchange, char *err, uint32_t cap) {
    uint32_t resident_slot, offscreen_slot;
    dc_liquid_seam_axis_t axis;
    bool resident_first;
    if (!view || !view->gpu || !exchange || view->closing || view->liquid_seam_failed ||
        !shared_face(exchange->resident, exchange->offscreen, &axis, &resident_first) ||
        !dc_chunk_table_find(&view->table, exchange->resident, &resident_slot) ||
        resident_slot >= dc_gpu_slot_capacity(view->gpu)) {
        error(err, cap, "Liquid seam has no adjacent resident owner");
        return DC_LIQUID_SEAM_REJECTED;
    }
    dc_chunk_slot_t *resident = &view->table.slots[resident_slot];
    dc_level_offscreen_handoff_t *handoff = dc_level_offscreen_lookup(view, exchange->offscreen);
    if (resident->state != DC_SLOT_ACTIVE || resident->pinned ||
        resident->generation != exchange->resident_generation || dc_gpu_liquid_owner_held(view->gpu) ||
        !handoff || !handoff->admitted || dc_gpu_liquid_owner_held(handoff->workspace->gpu) ||
        handoff->generation != exchange->offscreen_generation || handoff->workspace->failed ||
        handoff->workspace->held_writes || !dc_offscreen_page_at(handoff->workspace, exchange->offscreen,
            &offscreen_slot) || handoff->workspace->generations[offscreen_slot] != handoff->generation) {
        error(err, cap, "Liquid seam owner is stale or busy");
        return DC_LIQUID_SEAM_REJECTED;
    }
    dc_offscreen_t *workspace = handoff->workspace;
    const uint32_t face = (uint32_t)axis * 2u + (resident_first ? 0u : 1u);
    const uint32_t resident_page = view->gpu->slot_page[resident_slot];
    if (resident_page == UINT32_MAX || !view->gpu->page_width ||
        !aligned_interval(view, resident, handoff, exchange, face)) {
        error(err, cap, "Liquid seam interval is stale or its owners have different times");
        return DC_LIQUID_SEAM_REJECTED;
    }
    const dc_liquid_seam_endpoint_t local = {.gpu = view->gpu, .coord = exchange->resident,
        .tile_x = resident_page % view->gpu->page_width, .tile_y = resident_page / view->gpu->page_width,
        .slot = resident_slot};
    const dc_liquid_seam_endpoint_t remote = {.gpu = workspace->gpu, .coord = exchange->offscreen,
        .tile_x = offscreen_slot % OFFSCREEN_SIDE, .tile_y = offscreen_slot / OFFSCREEN_SIDE,
        .slot = offscreen_slot};
    const dc_liquid_seam_endpoint_t first = resident_first ? local : remote;
    const dc_liquid_seam_endpoint_t second = resident_first ? remote : local;
    if (!dc_gpu_liquid_seam_mapped(first, second, axis)) {
        error(err, cap, "Liquid seam endpoints no longer share their mapped face");
        return DC_LIQUID_SEAM_REJECTED;
    }
    if (!dc_level_view_stage_world_save(view, err, cap)) return DC_LIQUID_SEAM_REJECTED;
    resident->pinned = true;
    ++workspace->held_writes;
    dc_liquid_seam_result_t result = dc_gpu_commit_liquid_seam(first, second, axis, exchange->flux, err, cap);
    if (result == DC_LIQUID_SEAM_UNCONFIRMED) {
        result = dc_gpu_resolve_liquid_seam(first.gpu, second.gpu, err, cap);
        if (result == DC_LIQUID_SEAM_COMMITTED && err && cap) err[0] = '\0';
    }
    if (result == DC_LIQUID_SEAM_COMMITTED) {
        handoff->liquid_face_end[face] = exchange->end_seconds;
        handoff->liquid_paired = true;
        dc_chunk_table_mark_dirty(&view->table, resident_slot);
    } else if (result == DC_LIQUID_SEAM_UNCONFIRMED) {
        view->liquid_seam_failed = true;
        workspace->failed = true;
    }
    --workspace->held_writes;
    resident->pinned = false;
    return result;
}

bool dc_level_offscreen_flush_pairs(dc_level_view_t *view, char *err, uint32_t cap) {
    if (view->liquid_seam_failed) return error(err, cap, "Cannot publish an unconfirmed liquid seam");
    for (dc_level_offscreen_handoff_t *lease = view->offscreen_handoffs; lease; lease = lease->next) {
        if (lease->liquid_paired && !lease->write_generation &&
            !dc_level_offscreen_publish(view, lease->coord, lease->generation, err, cap)) return false;
    }
    return true;
}
