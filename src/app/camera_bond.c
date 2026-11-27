#include <stdio.h>
#include <stdlib.h>

#include "../vulkan/gpu_internal.h"
#include "level_internal.h"
#include "offscreen_internal.h"

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static unsigned page_index(const dc_offscreen_bond_group_t *group, dc_chunk_coord_t coord) {
    for (unsigned i = 0; i < group->page_count; ++i)
        if (group->pages[i].x == coord.x && group->pages[i].y == coord.y) return i;
    return group->page_count;
}

static bool needed(const dc_level_view_t *view, const dc_offscreen_bond_group_t *group) {
    if (view->transfer_pending && page_index(group, view->transfer_to) < group->page_count) return true;
    for (unsigned i = 0; i < group->page_count; ++i)
        if (dc_level_chunk_visible(view, group->pages[i])) return true;
    return false;
}

bool dc_camera_bond_offer(dc_level_view_t *view, const dc_stream_result_t *source,
    dc_chunk_owner_t owner, uint64_t generation) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (!source->chunk || !source->time.known || source->time.seconds != view->world_seconds ||
        !dc_chunk_owner_valid(owner)) return false;
    if (!camera->bond) {
        dc_offscreen_bond_group_t group;
        if (!dc_offscreen_bond_group_at(view->gpu, source->coord, &group)) return false;
        unsigned occupied = group.page_count;
        if (needed(view, &group) || !dc_camera_bond_enclose(view, &group) ||
            !dc_camera_bond_page_sealed(&group, source)) return false;
        dc_camera_bond_t *bond = calloc(1, sizeof(*bond));
        if (!bond) return false;
        bond->group = group;
        bond->occupied_pages = occupied;
        for (unsigned i = 0; i < group.page_count; ++i) {
            uint32_t slot;
            if (!dc_chunk_table_find(&view->table, group.pages[i], &slot)) {
                free(bond);
                return false;
            }
            const dc_chunk_slot_t *s = &view->table.slots[slot];
            if (!s->dirty || s->pinned || !s->time.known || s->time.seconds != source->time.seconds) {
                free(bond);
                return false;
            }
            if (s->state == DC_SLOT_SAVING && dc_chunk_owner_valid(s->checkpoint_owner)) {
                bond->receipts[i].owner = s->checkpoint_owner;
                bond->receipts[i].save_generation = s->save_generation;
            }
        }
        *camera = (dc_camera_offscreen_t){.bond = bond, .coord = group.pages[0], .time = source->time,
            .phase = DC_CAMERA_OFFSCREEN_SAVING, .rigid_group = true};
    }
    dc_camera_bond_t *bond = camera->bond;
    unsigned i = page_index(&bond->group, source->coord);
    if (camera->phase != DC_CAMERA_OFFSCREEN_SAVING || i == bond->group.page_count) return true;
    if (!dc_camera_bond_page_sealed(&bond->group, source)) {
        dc_camera_offscreen_discard(view);
        return true;
    }
    bond->receipts[i].owner = owner;
    bond->receipts[i].save_generation = generation;
    bond->receipts[i].saved = false;
    return true;
}

void dc_camera_bond_saved(dc_level_view_t *view, const dc_stream_result_t *result) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    dc_camera_bond_t *bond = camera->bond;
    unsigned i = page_index(&bond->group, result->coord);
    if (camera->phase != DC_CAMERA_OFFSCREEN_SAVING || i == bond->group.page_count ||
        bond->receipts[i].save_generation != result->generation) return;
    bond->receipts[i].saved = result->kind == DC_STREAM_SAVED &&
        dc_chunk_owner_equal(result->replaces_owner, bond->receipts[i].owner);
    for (unsigned j = 0; j < bond->group.page_count; ++j)
        if (!bond->receipts[j].saved) return;
    camera->phase = DC_CAMERA_OFFSCREEN_STORED;
}

static void release_source(dc_level_view_t *view, bool retry) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    dc_offscreen_release_workspace_leases(view, camera->workspace);
    if (retry) dc_camera_offscreen_retry(camera);
    else dc_camera_offscreen_discard(view);
}

static bool return_group(dc_level_view_t *view, char *err, uint32_t cap) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    dc_camera_bond_t *bond = camera->bond;
    dc_offscreen_bond_group_t current;
    if (!dc_offscreen_collect_bond_group(camera->workspace->gpu, bond->group.members[0].id, &current) ||
        current.member_count != bond->group.member_count) return error(err, cap, "Bonded ownership changed");
    for (unsigned i = 0; i < current.member_count; ++i) {
        const dc_offscreen_bond_member_t *m = &current.members[i];
        unsigned j = 0;
        while (j < bond->group.member_count && bond->group.members[j].id != m->id) ++j;
        if (j == bond->group.member_count || bond->group.members[j].member_generation != m->member_generation)
            return error(err, cap, "Bonded member generation changed");
    }
    if (!dc_offscreen_move_bond_group(camera->workspace->gpu, view->gpu, &current, err, cap)) return false;
    view->closing_structure_saved = false;
    release_source(view, false);
    return true;
}

bool dc_camera_bond_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    dc_camera_bond_t *bond = camera->bond;
    bool reclaim = closing || needed(view, &bond->group);
    if (!bond->owns && reclaim) {
        release_source(view, false);
        return true;
    }
    if (camera->phase == DC_CAMERA_OFFSCREEN_SAVING) {
        for (unsigned i = 0; i < bond->group.page_count; ++i)
            if (!bond->receipts[i].save_generation) {
                dc_camera_offscreen_discard(view);
                break;
            }
        return true;
    }
    if (camera->phase == DC_CAMERA_OFFSCREEN_STORED) {
        dc_chunk_coord_t origin = camera->coord;
        for (unsigned i = 0; i < bond->group.page_count; ++i) {
            dc_chunk_coord_t p = bond->group.pages[i];
            if (dc_chunk_table_find(&view->table, p, NULL)) return true;
            if (p.x < origin.x) origin.x = p.x;
            if (p.y < origin.y) origin.y = p.y;
        }
        camera->workspace = dc_offscreen_create(view->gpu, origin, err, cap);
        if (!camera->workspace) return false;
        for (unsigned i = 0; i < bond->group.page_count; ++i)
            if (!dc_level_offscreen_begin_disk(view, camera->workspace, bond->group.pages[i],
                    bond->receipts[i].owner, camera->time, &bond->receipts[i].lease_generation, err, cap)) {
                release_source(view, true);
                return false;
            }
        camera->generation = bond->receipts[0].lease_generation;
        camera->phase = DC_CAMERA_OFFSCREEN_LEASED;
    }
    if (!bond->owns || (!reclaim && !bond->returning)) return true;
    bond->returning = true;
    for (unsigned i = 0; i < bond->group.page_count; ++i) {
        if (bond->receipts[i].returned) continue;
        dc_chunk_coord_t p = bond->group.pages[i];
        dc_level_offscreen_handoff_t *lease = dc_level_offscreen_lookup(view, p);
        if (!lease || lease->generation != bond->receipts[i].lease_generation)
            return error(err, cap, "Bonded page lease changed");
        if (!lease->write_generation && !dc_level_offscreen_publish(view, p, lease->generation, err, cap))
            return false;
    }
    for (unsigned i = 0; i < bond->group.page_count; ++i)
        if (!bond->receipts[i].returned) return true;
    return return_group(view, err, cap);
}

dc_level_offscreen_result_t dc_camera_bond_result(dc_level_view_t *view, dc_level_offscreen_handoff_t *lease,
    const dc_stream_result_t *result, char *err, uint32_t cap) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    dc_camera_bond_t *bond = camera->bond;
    unsigned i = page_index(&bond->group, result->coord);
    if (i == bond->group.page_count) return DC_LEVEL_OFFSCREEN_UNMATCHED;
    if (!lease->admitted && result->generation == lease->generation) {
        if (result->kind != DC_STREAM_LOADED || !dc_chunk_owner_equal(result->replaces_owner, lease->owner) ||
            !result->time.known || result->time.seconds != camera->time.seconds ||
            !dc_camera_bond_page_sealed(&bond->group, result) ||
            !dc_offscreen_admit(camera->workspace, result, lease->generation, err, cap)) {
            release_source(view, true);
            error(err, cap, "Bonded workspace source load failed");
            return DC_LEVEL_OFFSCREEN_FAILED;
        }
        lease->admitted = true;
        for (unsigned j = 0; j < bond->group.page_count; ++j) {
            dc_level_offscreen_handoff_t *page = dc_level_offscreen_lookup(view, bond->group.pages[j]);
            if (!page || !page->admitted) return DC_LEVEL_OFFSCREEN_HANDLED;
        }
        dc_offscreen_bond_group_t physical = bond->group;
        physical.page_count = bond->occupied_pages;
        if (!dc_level_view_stage_world_save(view, err, cap) ||
            !dc_offscreen_move_bond_group(view->gpu, camera->workspace->gpu, &physical, err, cap)) {
            release_source(view, true);
            return DC_LEVEL_OFFSCREEN_FAILED;
        }
        camera->workspace->gpu->rigid_step_budget = view->gpu->rigid_step_budget;
        for (unsigned j = 0; j < bond->group.page_count; ++j)
            dc_level_offscreen_lookup(view, bond->group.pages[j])->owns_bodies = true;
        bond->owns = true;
        return DC_LEVEL_OFFSCREEN_HANDLED;
    }
    if (!lease->write_generation || lease->write_generation != result->generation)
        return DC_LEVEL_OFFSCREEN_UNMATCHED;
    if (result->kind != DC_STREAM_FAILED && (result->kind != DC_STREAM_SAVED ||
        !dc_chunk_owner_equal(result->replaces_owner, lease->owner))) return DC_LEVEL_OFFSCREEN_UNMATCHED;
    --lease->workspace->held_writes;
    lease->write_generation = 0;
    if (result->kind == DC_STREAM_FAILED) {
        error(err, cap, "Bonded workspace replacement write failed");
        return DC_LEVEL_OFFSCREEN_FAILED;
    }
    bond->receipts[i].returned = true;
    for (unsigned j = 0; j < bond->group.page_count; ++j)
        if (!bond->receipts[j].returned) return DC_LEVEL_OFFSCREEN_HANDLED;
    return return_group(view, err, cap) ? DC_LEVEL_OFFSCREEN_HANDLED : DC_LEVEL_OFFSCREEN_FAILED;
}
