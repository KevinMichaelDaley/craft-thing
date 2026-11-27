#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../vulkan/revolute_gpu_internal.h"
#include "../vulkan/runtime_body_gpu.h"
#include "offscreen_internal.h"

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static int64_t chunk_coordinate(const uint32_t words[2]) {
    uint64_t bits = words[0] | ((uint64_t)words[1] << 32);
    int64_t chunk;
    memcpy(&chunk, &bits, sizeof(chunk));
    return chunk;
}

static int nearby_delta(const uint32_t words[2], int64_t origin) {
    int64_t chunk = chunk_coordinate(words);
    if (chunk == origin) return 0;
    if (chunk > origin) return chunk - 1 == origin ? 1 : 2;
    return chunk + 1 == origin ? -1 : -2;
}

typedef enum { OUTSIDE_ROOM, INSIDE_ROOM, SPANS_ROOM } room_relation_t;

static room_relation_t room_relation(const dc_gpu_body_record_t *body, dc_chunk_coord_t coord) {
    int dx = nearby_delta(body->chunk_x, coord.x), dy = nearby_delta(body->chunk_y, coord.y);
    if (dx == 2 || dx == -2 || dy == 2 || dy == -2) return OUTSIDE_ROOM;
    const float px = dx * DC_CHUNK_SIDE + (float)body->local_x_fp / DC_FLUID_FULL;
    const float py = dy * DC_CHUNK_SIDE + (float)body->local_y_fp / DC_FLUID_FULL;
    const float c = cosf(body->angle), s = sinf(body->angle);
    float xmin = INFINITY, ymin = INFINITY, xmax = -INFINITY, ymax = -INFINITY;
    uint32_t pieces = body->piece_count ? body->piece_count : 1u;
    for (uint32_t p = 0; p < pieces; ++p) {
        const dc_gpu_body_shape_t *shape = body->piece_count ? body->pieces + p : &body->shape;
        for (uint32_t i = 0; i < (shape->count ? shape->count : 4u); ++i) {
            float x = shape->count ? (float)shape->vertices[i].x_fp / DC_FLUID_FULL :
                (i == 1u || i == 2u ? (float)body->body.width : 0.f);
            float y = shape->count ? (float)shape->vertices[i].y_fp / DC_FLUID_FULL :
                (i >= 2u ? (float)body->body.height : 0.f);
            x -= body->center[0];
            y -= body->center[1];
            float wx = px + body->center[0] + c*x - s*y;
            float wy = py + body->center[1] + s*x + c*y;
            xmin = fminf(xmin, wx); xmax = fmaxf(xmax, wx);
            ymin = fminf(ymin, wy); ymax = fmaxf(ymax, wy);
        }
    }
    if (xmax <= 0 || ymax <= 0 || xmin >= DC_CHUNK_SIDE || ymin >= DC_CHUNK_SIDE) return OUTSIDE_ROOM;
    if (xmin >= 0 && ymin >= 0 && xmax <= DC_CHUNK_SIDE && ymax <= DC_CHUNK_SIDE) return INSIDE_ROOM;
    return SPANS_ROOM;
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
        room_relation_t relation = room_relation(record, coord);
        if (relation == SPANS_ROOM) return false;
        if (relation == INSIDE_ROOM) found = true;
    }
    return found;
}

static bool free_slot(const dc_gpu_t *gpu, uint32_t i) {
    const dc_body_storage_owner_t *owner = dc_gpu_body_owner(gpu, i);
    return !gpu->body_ids[i] && owner->generation != UINT32_MAX &&
        owner->state != DC_BODY_OWNER_RETIRED && owner->state != DC_BODY_OWNER_RESERVED;
}

static bool add_page(dc_offscreen_bond_group_t *group, dc_chunk_coord_t page) {
    for (uint32_t i = 0; i < group->page_count; ++i) {
        dc_chunk_coord_t p = group->pages[i];
        if (p.x == page.x && p.y == page.y) return true;
        uint64_t dx = p.x > page.x ? (uint64_t)p.x - page.x : (uint64_t)page.x - p.x;
        uint64_t dy = p.y > page.y ? (uint64_t)p.y - page.y : (uint64_t)page.y - p.y;
        if (dx >= OFFSCREEN_SIDE || dy >= OFFSCREEN_SIDE) return false;
    }
    if (group->page_count == OFFSCREEN_SLOTS) return false;
    group->pages[group->page_count++] = page;
    return true;
}

static bool add_member(dc_gpu_t *gpu, dc_offscreen_bond_group_t *group, uint32_t slot, uint32_t id) {
    if (slot >= dc_gpu_body_capacity(gpu)) return false;
    const dc_gpu_body_record_t *body = dc_gpu_body_record(gpu, slot);
    if (body->body.id != id || !body->body.active || body->reserved[0] != DC_GPU_STRUCTURAL_CONVERTED_TAG ||
        !body->reserved[1] || body->piece_count > DC_GPU_BODY_PIECES) return false;
    for (uint32_t i = 0; i < group->member_count; ++i)
        if (group->members[i].id == id) return group->members[i].slot == slot;
    if (group->member_count == 2u * DC_OFFSCREEN_BOND_CAPACITY) return false;
    group->members[group->member_count++] = (dc_offscreen_bond_member_t){slot, id, body->reserved[1]};
    int64_t x = chunk_coordinate(body->chunk_x), y = chunk_coordinate(body->chunk_y);
    for (int dy = -1; dy <= 1; ++dy) {
        if ((dy < 0 && y == INT64_MIN) || (dy > 0 && y == INT64_MAX)) continue;
        for (int dx = -1; dx <= 1; ++dx) {
            if ((dx < 0 && x == INT64_MIN) || (dx > 0 && x == INT64_MAX)) continue;
            dc_chunk_coord_t page = {x + dx, y + dy};
            if (room_relation(body, page) != OUTSIDE_ROOM && !add_page(group, page)) return false;
        }
    }
    return true;
}

bool dc_offscreen_collect_bond_group(dc_gpu_t *gpu, uint32_t id, dc_offscreen_bond_group_t *group) {
    if (!gpu || !id || !group || !gpu->structural_graph_possible) return false;
    dc_offscreen_bond_group_t found = {0};
    const uint32_t *bonds = dc_gpu_structural_data(gpu) + dc_gpu_structural_graph_offset(gpu) + 30000u;
    uint32_t slot = 0;
    while (slot < gpu->body_count && gpu->body_ids[slot] != id) ++slot;
    if (slot == gpu->body_count || !add_member(gpu, &found, slot, id)) return false;
    for (uint32_t i = 0; i < found.member_count; ++i) {
        dc_offscreen_bond_member_t member = found.members[i];
        for (uint32_t j = 0; j < DC_OFFSCREEN_BOND_CAPACITY; ++j) {
            const uint32_t *bond = bonds + j * 20u;
            if (!bond[0] || (bond[3] != member.id && bond[4] != member.id)) continue;
            unsigned end = bond[3] == member.id ? 0u : 1u;
            if (bond[1u + end] != member.slot || bond[3] == bond[4] ||
                !add_member(gpu, &found, bond[2u - end], bond[4u - end])) return false;
        }
    }
    if (found.member_count < 2u || !found.page_count) return false;
    *group = found;
    return true;
}

static void move_words(uint32_t *to, uint32_t *from, size_t count) {
    memcpy(to, from, count * sizeof(*from));
    memset(from, 0, count * sizeof(*from));
}

static void move_record(dc_gpu_t *from, uint32_t i, dc_gpu_t *to, uint32_t next) {
    dc_gpu_body_record_t *source = dc_gpu_body_record(from, i);
    *dc_gpu_body_record(to, next) = *source;
    move_words(dc_gpu_body_solver_state(to, next), dc_gpu_body_solver_state(from, i),
        from->runtime_body_consumers ? 64u : DC_GPU_SOLVER_BODY_WORDS);
    move_words(dc_gpu_body_hydro_state(to, next), dc_gpu_body_hydro_state(from, i), 128u);
    if (!from->runtime_body_consumers) {
        uint32_t *a = dc_gpu_structural_data(from) + dc_gpu_structural_graph_offset(from);
        uint32_t *b = dc_gpu_structural_data(to) + dc_gpu_structural_graph_offset(to);
        const unsigned offsets[] = {36000u, 38000u, 38500u};
        for (unsigned j = 0; j < 3u; ++j)
            move_words(b + offsets[j] + next * 4u, a + offsets[j] + i * 4u, 4u);
    }
    to->body_ids[next] = source->body.id;
    from->body_ids[i] = 0;
    dc_body_storage_owner_t *owner = dc_gpu_body_owner(from, i), *dest = dc_gpu_body_owner(to, next);
    uint32_t generation = dest->generation + 1u;
    *dest = *owner;
    dest->generation = generation;
    dest->runtime_id = source->body.id;
    dest->state = DC_BODY_OWNER_LIVE;
    dest->live_index = next;
    *owner = (dc_body_storage_owner_t){.generation = owner->generation,
        .state = owner->generation == UINT32_MAX ? DC_BODY_OWNER_RETIRED : DC_BODY_OWNER_FREE};
    memset(source, 0, sizeof(*source));
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
        if (!from->body_ids[i] || room_relation(dc_gpu_body_record(from, i), coord) != INSIDE_ROOM) continue;
        if (dc_gpu_find_body_record(to, from->body_ids[i])) return error(err, cap, "Rigid handoff ID already exists");
        ++needed;
    }
    for (uint32_t i = 0; i < capacity; ++i) available += free_slot(to, i);
    if (available < needed && (needed-available > UINT32_MAX-capacity ||
        !dc_gpu_runtime_bodies_reserve(to, capacity + (needed-available), err, cap))) return false;

    uint32_t next = 0;
    for (uint32_t i = 0; i < from->body_count; ++i) {
        if (!from->body_ids[i] || room_relation(dc_gpu_body_record(from, i), coord) != INSIDE_ROOM) continue;
        while (!free_slot(to, next)) ++next;
        move_record(from, i, to, next++);
    }
    dc_gpu_refresh_body_registry(from);
    dc_gpu_refresh_body_registry(to);
    from->body_refresh_pending = to->body_refresh_pending = true;
    return true;
}

bool dc_offscreen_move_bond_group(dc_gpu_t *from, dc_gpu_t *to, const dc_offscreen_bond_group_t *group,
    char *err, uint32_t cap) {
    if (!from || !to || from == to || from->device_lifetime != to->device_lifetime ||
        from->runtime_body_consumers != to->runtime_body_consumers || !group || !group->member_count ||
        fr_gpu_revolute_span(from) || vkQueueWaitIdle(from->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot synchronize bonded assembly handoff");
    dc_gpu_refresh_body_registry(from);
    dc_gpu_refresh_body_registry(to);
    dc_offscreen_bond_group_t confirmed;
    if (!dc_offscreen_collect_bond_group(from, group->members[0].id, &confirmed) ||
        confirmed.member_count != group->member_count || confirmed.page_count != group->page_count ||
        memcmp(confirmed.members, group->members, confirmed.member_count * sizeof(*group->members)) ||
        memcmp(confirmed.pages, group->pages, confirmed.page_count * sizeof(*group->pages)))
        return error(err, cap, "Bonded assembly changed before handoff");
    uint32_t *a = dc_gpu_structural_data(from) + dc_gpu_structural_graph_offset(from);
    fr_structural_gpu_graph_t *graph = (void *)a;
    if (to->structural_graph_possible || graph->member_count != group->member_count ||
        graph->member_count > FR_STRUCTURAL_MEMBER_CAPACITY)
        return error(err, cap, "Cannot move a partial authored graph");
    uint32_t dest_slots[FR_STRUCTURAL_MEMBER_CAPACITY], capacity = dc_gpu_body_capacity(to), available = 0;
    for (uint32_t i = 0; i < group->member_count; ++i) {
        const fr_structural_gpu_member_t *m = &graph->members[i];
        const dc_gpu_body_record_t *body = m->reserved < from->body_count ? dc_gpu_body_record(from, m->reserved) : NULL;
        if (!body || !(m->flags & 2u) || body->body.id != FR_STRUCTURAL_BODY_ID_BASE + m->id ||
            body->reserved[1] != m->generation || dc_gpu_find_body_record(to, body->body.id) ||
            (body->motion_flags & (DC_BODY_MOTION_REACTIVE_MASS | DC_BODY_MOTION_THERMAL_GRID)))
            return error(err, cap, "Invalid converted member or destination identity");
        uint32_t j = 0;
        while (j < group->member_count && group->members[j].slot != m->reserved) ++j;
        if (j == group->member_count) return error(err, cap, "Authored member belongs to another assembly");
    }
    for (uint32_t i = 0; i < DC_OFFSCREEN_BOND_CAPACITY; ++i) {
        const uint32_t *bond = a + 30000u + 20u * i;
        if (!bond[0]) continue;
        for (unsigned end = 0; end < 2; ++end) {
            uint32_t j = 0;
            while (j < group->member_count && group->members[j].slot != bond[1u + end]) ++j;
            if (j == group->member_count || group->members[j].id != bond[3u + end])
                return error(err, cap, "An external bond requires another owner");
        }
    }
    for (uint32_t i = 0; i < capacity; ++i) available += free_slot(to, i);
    if (available < group->member_count) {
        uint32_t extra = group->member_count - available;
        if (extra > UINT32_MAX - capacity || !dc_gpu_runtime_bodies_reserve(to, capacity + extra, err, cap)) return false;
    }
    if (!dc_gpu_solver_graph_prepare(to, err, cap) || !dc_gpu_set_rigid_solver(to, true)) return false;
    uint32_t *b = dc_gpu_structural_data(to) + dc_gpu_structural_graph_offset(to);
    uint32_t next = 0;
    for (uint32_t i = 0; i < graph->member_count; ++i) {
        while (!free_slot(to, next)) ++next;
        dest_slots[i] = next++;
    }
    for (uint32_t i = 0; i < DC_OFFSCREEN_BOND_CAPACITY; ++i) {
        uint32_t *old = a + 30000u + 20u * i, *bond = b + 30000u + 20u * i;
        if (!old[0]) continue;
        move_words(bond, old, 20u);
        for (unsigned end = 0; end < 2; ++end)
            for (uint32_t j = 0; j < graph->member_count; ++j)
                if (graph->members[j].reserved == bond[1u + end]) {
                    bond[1u + end] = dest_slots[j];
                    break;
                }
    }
    for (uint32_t i = 0; i < graph->member_count; ++i) {
        move_record(from, graph->members[i].reserved, to, dest_slots[i]);
        graph->members[i].reserved = dest_slots[i];
    }
    const struct { unsigned start, words; } ranges[] = {
        {0, DC_STRUCTURAL_GRAPH_WORDS},
        {2u * DC_STRUCTURAL_GRAPH_WORDS, FR_STRUCTURAL_NODE_CAPACITY * DC_STRUCTURAL_NODE_STATE_WORDS},
        {24000u, FR_STRUCTURAL_MEMBER_CAPACITY * 16u}, {24400u, FR_STRUCTURAL_NODE_CAPACITY * 16u},
        {28000u, FR_STRUCTURAL_NODE_CAPACITY * 4u}, {28300u, FR_STRUCTURAL_JOINT_CAPACITY + FR_STRUCTURAL_ANCHOR_CAPACITY}
    };
    for (unsigned i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i)
        move_words(b + ranges[i].start, a + ranges[i].start, ranges[i].words);
    uint32_t *old_control = dc_gpu_structural_data(from), *new_control = dc_gpu_structural_data(to);
    new_control[0] = old_control[0];
    old_control[0] = old_control[9] = 0;
    new_control[9] = 1;
    old_control[10] = old_control[11] = new_control[10] = new_control[11] = 1;
    if (new_control[14] < old_control[14]) new_control[14] = old_control[14];
    from->structural_graph_possible = false;
    to->structural_graph_possible = to->structural_enabled = true;
    dc_gpu_refresh_body_registry(from);
    dc_gpu_refresh_body_registry(to);
    from->body_refresh_pending = to->body_refresh_pending = true;
    return true;
}
