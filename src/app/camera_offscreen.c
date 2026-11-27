#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gas.h"
#include "dungeoncraft/terrain_flags.h"
#include "../vulkan/gpu_internal.h"
#include "camera_offscreen.h"
#include "level_internal.h"
#include "level_offscreen.h"
#include "offscreen_internal.h"

enum { CAMERA_WORKSPACE_STEPS_PER_TICK = 2u };

static bool same_coord(dc_chunk_coord_t a, dc_chunk_coord_t b) {
    return a.x == b.x && a.y == b.y;
}

static bool gas_cell_has_stock(const fr_gas_checkpoint_chunk_t *gas, uint32_t index) {
    if (!gas) return false;
    if (gas->carrier_tail[index][0] || gas->carrier_tail[index][1]) return true;
    for (uint32_t species = 0; species < FR_GAS_SPECIES; ++species) {
        if (gas->cells[index][species]) return true;
    }
    return false;
}

static bool cell_has_transport_stock(const dc_stream_result_t *snapshot, uint32_t index) {
    return snapshot->chunk->cells[index].fluid_mass ||
        (snapshot->bulk && (snapshot->bulk->oil_fp[index] || snapshot->bulk->slime_fp[index])) ||
        gas_cell_has_stock(snapshot->gas, index);
}

static bool has_transport_stock(const dc_stream_result_t *snapshot) {
    if (snapshot->chunk->particle_count) return true;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        if (cell_has_transport_stock(snapshot, i)) return true;
    }
    return false;
}

static bool closed_edge_cell(const dc_stream_result_t *snapshot, uint32_t index) {
    const dc_cell_t *cell = snapshot->chunk->cells + index;
    return cell->material == DC_MATERIAL_STONE && (cell->flags & DC_CELL_LOADED_TERRAIN) &&
        !(cell->flags & DC_CELL_RELEASED_TERRAIN) &&
        (!snapshot->gas || !snapshot->gas->cells[index][FR_GAS_CHECKPOINT_VOLUME_WORD]) &&
        !cell_has_transport_stock(snapshot, index);
}

bool dc_camera_offscreen_isolated(const dc_level_view_t *view, const dc_stream_result_t *snapshot) {
    if (!snapshot->chunk || !snapshot->time.known || !isfinite(snapshot->time.seconds) ||
        snapshot->time.seconds < 0. || (!dc_gpu_chunk_physics_isolated(view->gpu) &&
        !dc_offscreen_body_island(view->gpu, snapshot->coord)) ||
        snapshot->water_excess || snapshot->chunk->particle_count ||
        (snapshot->gas != NULL) != (view->gpu->gas != NULL)) return false;
    for (uint32_t i = 0; i < DC_CHUNK_SIDE; ++i) {
        if (!closed_edge_cell(snapshot, i) ||
            !closed_edge_cell(snapshot, DC_CHUNK_CELLS - DC_CHUNK_SIDE + i) ||
            !closed_edge_cell(snapshot, i * DC_CHUNK_SIDE) ||
            !closed_edge_cell(snapshot, (i + 1u) * DC_CHUNK_SIDE - 1u)) return false;
    }
    return true;
}

enum { WEST = 1, EAST = 2, NORTH = 4, SOUTH = 8 };

static bool face_neighbor(dc_chunk_coord_t coord, unsigned face, dc_chunk_coord_t *neighbor) {
    if (face == WEST && coord.x > INT64_MIN) --coord.x;
    else if (face == EAST && coord.x < INT64_MAX) ++coord.x;
    else if (face == NORTH && coord.y > INT64_MIN) --coord.y;
    else if (face == SOUTH && coord.y < INT64_MAX) ++coord.y;
    else return false;
    *neighbor = coord;
    return true;
}

static bool face_in_view(const dc_level_view_t *view, dc_chunk_coord_t coord, unsigned face) {
    dc_chunk_coord_t neighbor;
    return face_neighbor(coord, face, &neighbor) && dc_level_chunk_visible(view, neighbor);
}

static bool closed_face(const dc_stream_result_t *source, unsigned face) {
    for (unsigned i = 0; i < DC_CHUNK_SIDE; ++i) {
        unsigned index = face == NORTH ? i : face == SOUTH ? DC_CHUNK_CELLS - DC_CHUNK_SIDE + i :
            i * DC_CHUNK_SIDE + (face == EAST ? DC_CHUNK_SIDE - 1u : 0u);
        if (!closed_edge_cell(source, index)) return false;
    }
    return true;
}

bool dc_camera_bond_enclose(dc_level_view_t *view, dc_offscreen_bond_group_t *group) {
    for (unsigned i = 0; i < group->page_count; ++i) {
        uint32_t slot;
        dc_chunk_t chunk = {.coord = group->pages[i]};
        if (dc_level_chunk_visible(view, chunk.coord) || !dc_chunk_table_find(&view->table, chunk.coord, &slot) ||
            !dc_gpu_download_chunk(view->gpu, slot, &chunk, NULL, 0)) return false;
        const dc_stream_result_t source = {.chunk = &chunk,
            .bulk = (fr_bulk_phase_chunk_t *)view->gpu->bulk_phase_mapped + slot};
        for (unsigned face = WEST; face <= SOUTH; face <<= 1) {
            if (closed_face(&source, face)) continue;
            dc_chunk_coord_t next;
            if (!face_neighbor(chunk.coord, face, &next) || !dc_offscreen_bond_add_page(group, next)) return false;
        }
    }
    return dc_offscreen_bond_pages_clear(view->gpu, group);
}

bool dc_camera_bond_page_sealed(const dc_offscreen_bond_group_t *group, const dc_stream_result_t *source) {
    if (!source->chunk || !source->time.known || source->gas || source->water_excess ||
        source->chunk->particle_count) return false;
    for (unsigned face = WEST; face <= SOUTH; face <<= 1) {
        dc_chunk_coord_t next;
        bool interior = false;
        if (face_neighbor(source->coord, face, &next))
            for (unsigned p = 0; p < group->page_count; ++p) interior |= same_coord(next, group->pages[p]);
        if (!interior && !closed_face(source, face)) return false;
    }
    return true;
}

static bool neighbor_has_transport_stock(dc_level_view_t *view, dc_chunk_coord_t coord, unsigned face) {
    dc_chunk_coord_t neighbor;
    uint32_t slot;
    if (!face_neighbor(coord, face, &neighbor) || !dc_chunk_table_find(&view->table, neighbor, &slot) ||
        view->table.slots[slot].state != DC_SLOT_ACTIVE ||
        !dc_gpu_copy_chunk_state(view->gpu, slot, false, false, NULL, 0)) return false;
    if (((const uint32_t *)view->gpu->particle_count_mapped)[slot]) return true;
    const dc_cell_t *cells = (const dc_cell_t *)view->gpu->chunk_mapped + (size_t)slot * DC_CHUNK_CELLS;
    const fr_bulk_phase_chunk_t *bulk = (const fr_bulk_phase_chunk_t *)view->gpu->bulk_phase_mapped + slot;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i)
        if (cells[i].fluid_mass || bulk->oil_fp[i] || bulk->slime_fp[i]) return true;
    if (view->gpu->gas && view->gas_admitted[slot]) {
        fr_gas_checkpoint_chunk_t *gas = malloc(sizeof(*gas));
        bool stock = false;
        if (gas && fr_gas_gpu_export_checkpoint(view->gpu, slot, gas, NULL, 0)) {
            for (unsigned i = 0; i < DC_CHUNK_CELLS && !stock; ++i)
                stock = gas_cell_has_stock(gas, i);
        }
        free(gas);
        return stock;
    }
    return false;
}

unsigned dc_camera_offscreen_water_face(const dc_level_view_t *view, const dc_stream_result_t *snapshot) {
    if (!snapshot->chunk || !snapshot->time.known || !isfinite(snapshot->time.seconds) ||
        snapshot->time.seconds < 0. || (snapshot->gas != NULL) != (view->gpu->gas != NULL))
        return 0;
    unsigned faces = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        const dc_cell_t *cell = &snapshot->chunk->cells[i];
        const uint32_t x = i % DC_CHUNK_SIDE, y = i / DC_CHUNK_SIDE;
        unsigned edge = (x == 0 ? WEST : 0) | (x == DC_CHUNK_SIDE - 1u ? EAST : 0) |
                        (y == 0 ? NORTH : 0) | (y == DC_CHUNK_SIDE - 1u ? SOUTH : 0);
        if (!edge || closed_edge_cell(snapshot, i)) continue;
        if (!DC_CELL_LIQUID_PERMEABLE(cell->material)) return 0;
        faces |= edge;
    }
    return !(faces & (faces - 1u)) ? faces : 0;
}

bool dc_camera_offscreen_ready(dc_level_view_t *view) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (camera->bond) return camera->bond->owns && !camera->bond->returning;
    if (!camera->water_face && !camera->rigid_group) return true;
    dc_chunk_time_t time;
    bool writing;
    return camera->phase == DC_CAMERA_OFFSCREEN_LEASED &&
        dc_level_offscreen_progress(view, camera->coord, camera->generation, &time, &writing) &&
        time.known && !writing;
}

bool dc_camera_offscreen_hold_remap(dc_level_view_t *view, bool *hold, char *err, uint32_t cap) {
    *hold = view->gpu->water_cycle_peer != NULL;
    if (*hold || !view->gpu->fluid_phase || (!view->gpu->fluid_work_possible && !view->gpu->gas) ||
        view->camera_offscreen.phase != DC_CAMERA_OFFSCREEN_IDLE) return true;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        const dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state != DC_SLOT_ACTIVE || !slot->time.known || slot->time.seconds != view->world_seconds ||
            dc_level_chunk_visible(view, slot->coord)) continue;
        unsigned face = WEST;
        while (face <= SOUTH && !face_in_view(view, slot->coord, face)) face <<= 1;
        if (face > SOUTH) continue;
        dc_chunk_t chunk;
        if (!dc_gpu_download_chunk(view->gpu, i, &chunk, err, cap)) return false;
        fr_gas_checkpoint_chunk_t *gas = NULL;
        if (view->gpu->gas && view->gas_admitted[i]) {
            gas = malloc(sizeof(*gas));
            if (!gas || !fr_gas_gpu_export_checkpoint(view->gpu, i, gas, err, cap)) {
                free(gas);
                return false;
            }
        }
        const dc_stream_result_t snapshot = {.gas = gas, .time = slot->time, .coord = slot->coord, .chunk = &chunk,
            .bulk = (fr_bulk_phase_chunk_t *)view->gpu->bulk_phase_mapped + i};
        if (dc_camera_offscreen_water_face(view, &snapshot) == face &&
            (has_transport_stock(&snapshot) || neighbor_has_transport_stock(view, slot->coord, face))) {
            *hold = true;
        }
        free(gas);
        if (*hold) break;
    }
    return true;
}

static dc_camera_offscreen_t *vacant_room(dc_level_view_t *view) {
    for (size_t i = 0; i < view->sealed_capacity; ++i)
        if (view->sealed_rooms[i].phase == DC_CAMERA_OFFSCREEN_IDLE) return view->sealed_rooms + i;
    return NULL;
}

bool dc_camera_offscreen_reserve(dc_level_view_t *view) {
    if (view->camera_offscreen.phase == DC_CAMERA_OFFSCREEN_IDLE || vacant_room(view)) return true;
    if (view->sealed_capacity > SIZE_MAX / sizeof(*view->sealed_rooms) / 2u) return false;
    size_t capacity = view->sealed_capacity ? 2u * view->sealed_capacity : DC_CAMERA_WORKSPACE_LIMIT;
    dc_camera_offscreen_t *rooms = realloc(view->sealed_rooms, capacity * sizeof(*rooms));
    if (!rooms) return false;
    memset(rooms + view->sealed_capacity, 0, (capacity - view->sealed_capacity) * sizeof(*rooms));
    view->sealed_rooms = rooms;
    view->sealed_capacity = capacity;
    return true;
}

dc_camera_offscreen_t *dc_camera_find_room(dc_level_view_t *view, dc_chunk_coord_t coord) {
    for (size_t i = 0; i <= view->sealed_capacity; ++i) {
        dc_camera_offscreen_t *room = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        if (room->phase != DC_CAMERA_OFFSCREEN_IDLE && same_coord(room->coord, coord)) return room;
    }
    return NULL;
}

bool dc_camera_offscreen_pending(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    for (size_t i = 0; i <= view->sealed_capacity; ++i) {
        const dc_camera_offscreen_t *room = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        if (room->phase == DC_CAMERA_OFFSCREEN_STORED && same_coord(room->coord, coord) &&
            (room->restored_record || room->time.seconds < view->world_seconds)) return true;
    }
    return false;
}

void dc_camera_offscreen_offer(dc_level_view_t *view, const dc_stream_result_t *snapshot,
    dc_chunk_owner_t owner, uint64_t write_generation) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (!view->closing && (camera->bond || camera->phase == DC_CAMERA_OFFSCREEN_IDLE) &&
        dc_camera_bond_offer(view, snapshot, owner, write_generation)) return;
    if (view->closing || !snapshot->chunk ||
        dc_level_chunk_visible(view, snapshot->coord) || !dc_chunk_owner_valid(owner)) return;
    unsigned face = 0;
    if (!dc_camera_offscreen_isolated(view, snapshot)) {
        face = dc_camera_offscreen_water_face(view, snapshot);
        if (!face || !face_in_view(view, snapshot->coord, face) || view->gpu->fluid_phase ||
            snapshot->time.seconds != view->world_seconds) return;
    }
    if (!has_transport_stock(snapshot) && !dc_offscreen_body_island(view->gpu, snapshot->coord) &&
        (!face || !neighbor_has_transport_stock(view, snapshot->coord, face))) return;
    bool rigid = !face && dc_offscreen_body_island(view->gpu, snapshot->coord);
    if (camera->phase != DC_CAMERA_OFFSCREEN_IDLE) {
        dc_camera_offscreen_t *spare = vacant_room(view);
        if (!spare) return;
        if (face || rigid) {
            if (camera->bond || camera->water_face || camera->rigid_group) return;
            *spare = *camera;
        } else {
            camera = spare;
        }
    }
    *camera = (dc_camera_offscreen_t){.coord = snapshot->coord, .owner = owner,
        .time = snapshot->time, .generation = write_generation, .phase = DC_CAMERA_OFFSCREEN_SAVING,
        .water_face = face, .rigid_group = rigid};
}

static void saved_room(dc_camera_offscreen_t *camera, const dc_stream_result_t *result) {
    if (camera->phase != DC_CAMERA_OFFSCREEN_SAVING || camera->generation != result->generation ||
        !same_coord(camera->coord, result->coord)) return;
    if (result->kind == DC_STREAM_FAILED) {
        *camera = (dc_camera_offscreen_t){0};
    } else if (result->kind == DC_STREAM_SAVED &&
               dc_chunk_owner_equal(result->replaces_owner, camera->owner)) {
        camera->phase = DC_CAMERA_OFFSCREEN_STORED;
    }
}

void dc_camera_offscreen_saved(dc_level_view_t *view, const dc_stream_result_t *result) {
    if (view->camera_offscreen.bond) dc_camera_bond_saved(view, result);
    else saved_room(&view->camera_offscreen, result);
    for (size_t i = 0; i < view->sealed_capacity; ++i)
        saved_room(view->sealed_rooms + i, result);
}

void dc_camera_offscreen_release(dc_camera_offscreen_t *camera) {
    free(camera->bond);
    dc_offscreen_destroy(camera->workspace);
    *camera = (dc_camera_offscreen_t){0};
}

void dc_camera_offscreen_discard(dc_level_view_t *view) {
    dc_camera_offscreen_release(&view->camera_offscreen);
    for (size_t i = 0; i < view->sealed_capacity; ++i)
        dc_camera_offscreen_release(view->sealed_rooms + i);
    free(view->sealed_rooms);
    view->sealed_rooms = NULL;
    view->sealed_capacity = 0;
}

void dc_camera_offscreen_retry(dc_camera_offscreen_t *camera) {
    dc_offscreen_destroy(camera->workspace);
    camera->workspace = NULL;
    camera->phase = DC_CAMERA_OFFSCREEN_STORED;
}

static bool camera_needs_chunk(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return dc_level_chunk_visible(view, coord) ||
        (view->transfer_pending && same_coord(view->transfer_to, coord));
}

static uint64_t axis_distance(int64_t coordinate, int64_t origin, unsigned width) {
    if (coordinate < origin) return (uint64_t)origin - (uint64_t)coordinate;
    uint64_t offset = (uint64_t)coordinate - (uint64_t)origin;
    return offset < width ? 0u : offset - width + 1u;
}

/* Cadence in nominal 60 Hz world ticks. Zero denotes a dormant room. */
static unsigned room_period(dc_chunk_coord_t coord, dc_chunk_coord_t origin) {
    uint64_t dx = axis_distance(coord.x, origin.x, VIEW_CHUNKS_X);
    uint64_t dy = axis_distance(coord.y, origin.y, VIEW_CHUNKS_Y);
    if (dx <= VIEW_CHUNKS_X && dy <= VIEW_CHUNKS_Y) return 4u;
    if (dx <= 2u * VIEW_CHUNKS_X && dy <= 2u * VIEW_CHUNKS_Y) return 12u;
    if (dx <= 4u * VIEW_CHUNKS_X && dy <= 4u * VIEW_CHUNKS_Y) return 24u;
    return 0u;
}

static dc_offscreen_t *shared_return_workspace(dc_level_view_t *view, const dc_camera_offscreen_t *room) {
    if (!dc_gpu_chunk_physics_isolated(view->gpu) || room->rigid_group || room->water_face) return NULL;
    for (size_t i = 0; i <= view->sealed_capacity; ++i) {
        const dc_camera_offscreen_t *other = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        dc_offscreen_t *workspace = other->workspace;
        uint32_t slot;
        if (!workspace || other->phase != DC_CAMERA_OFFSCREEN_LEASED || other->parking ||
            other->rigid_group || other->water_face || other->time.seconds != room->time.seconds ||
            !camera_needs_chunk(view, other->coord) || workspace->held_writes || workspace->failed ||
            (workspace->time.known && workspace->time.seconds != room->time.seconds) ||
            !dc_offscreen_page_at(workspace, room->coord, &slot) || workspace->generations[slot]) continue;
        ++workspace->references;
        return workspace;
    }
    return NULL;
}

static bool service_room(dc_level_view_t *view, dc_camera_offscreen_t *camera, bool closing,
    bool selected, bool capacity, char *err, uint32_t cap) {
    if (camera->bond) return dc_camera_bond_service(view, closing, err, cap);
    if (camera->phase == DC_CAMERA_OFFSCREEN_IDLE || camera->phase == DC_CAMERA_OFFSCREEN_SAVING)
        return true;
    bool needed = camera_needs_chunk(view, camera->coord);
    bool independent = dc_gpu_chunk_physics_isolated(view->gpu) || dc_offscreen_body_island(view->gpu, camera->coord);
    bool coupled = camera->water_face != 0u;
    bool attached = coupled && face_in_view(view, camera->coord, camera->water_face);
    unsigned period = room_period(camera->coord, view->origin);
    if (camera->parking && !dc_level_offscreen_owns(view, camera->coord)) {
        dc_camera_offscreen_retry(camera);
        camera->parking = false;
    }
    if (camera->phase == DC_CAMERA_OFFSCREEN_STORED) {
        if (closing && !coupled && !camera->rigid_group) return true;
        if (closing || (coupled ? !attached : !independent) || dc_chunk_table_find(&view->table, camera->coord, NULL) ||
            (needed && !camera->restored_record && camera->time.seconds >= view->world_seconds)) {
            dc_camera_offscreen_release(camera);
            return true;
        }
        if (!selected) return true;
        if (needed) camera->workspace = shared_return_workspace(view, camera);
        if (!camera->workspace) {
            if (!capacity) return true;
            camera->workspace = dc_offscreen_create(view->gpu, camera->coord, err, cap);
        }
        if (!camera->workspace || !dc_level_offscreen_begin_disk(view, camera->workspace,
            camera->coord, camera->owner, camera->time, &camera->generation, err, cap)) {
            dc_camera_offscreen_retry(camera);
            return false;
        }
        camera->phase = DC_CAMERA_OFFSCREEN_LEASED;
        if (coupled) {
            camera->workspace->gpu->fluid_tick = view->gpu->fluid_tick;
            camera->workspace->gpu->fluid_phase_budget = view->gpu->fluid_phase_budget;
        }
    }

    dc_chunk_time_t time;
    bool writing;
    if (!dc_level_offscreen_progress(view, camera->coord, camera->generation, &time, &writing)) {
        dc_camera_offscreen_release(camera);
        return true;
    }
    if (closing && time.known) camera->time = time;
    if (!time.known || writing) return true;
    if (camera->workspace->references > 1u)
        for (const dc_level_offscreen_handoff_t *lease = view->offscreen_handoffs; lease; lease = lease->next)
            if (lease->workspace == camera->workspace && !lease->admitted) return true;
    if (coupled || camera->rigid_group) {
        if (closing || needed || (coupled && !attached))
            return dc_level_offscreen_publish(view, camera->coord, camera->generation, err, cap);
        return true;
    }
    if (!closing && independent && !selected) {
        camera->time = time;
        camera->parking = dc_level_offscreen_publish(view, camera->coord, camera->generation, err, cap);
        return camera->parking;
    }
    camera->parking = false;
    if (needed || view->world_seconds - time.seconds >= period / 60.)
        camera->workspace->due_seconds = view->world_seconds;
    if (!closing && independent && time.seconds < camera->workspace->due_seconds &&
        !dc_offscreen_advance(camera->workspace, camera->workspace->due_seconds, CAMERA_WORKSPACE_STEPS_PER_TICK, err, cap)) {
        if (dc_level_offscreen_cancel(view, camera->coord, camera->generation, NULL, 0))
            dc_camera_offscreen_release(camera);
        return false;
    }
    dc_level_offscreen_progress(view, camera->coord, camera->generation, &time, &writing);
    if (closing || !independent || (needed && time.seconds >= view->world_seconds))
        return dc_level_offscreen_publish(view, camera->coord, camera->generation, err, cap);
    return true;
}

bool dc_camera_offscreen_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap) {
    dc_camera_offscreen_t *selected[DC_CAMERA_WORKSPACE_LIMIT] = {0};
    unsigned priorities[DC_CAMERA_WORKSPACE_LIMIT], count = 0, live = 0, returning = 0;
    for (size_t i = 0; i <= view->sealed_capacity; ++i) {
        dc_camera_offscreen_t *room = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        if (room->phase == DC_CAMERA_OFFSCREEN_IDLE) continue;
        bool needed = camera_needs_chunk(view, room->coord);
        live += room->workspace != NULL;
        returning += room->workspace && needed;
        unsigned priority = room->water_face || room->rigid_group || room->bond ? 0u :
            needed ? 1u : room_period(room->coord, view->origin);
        if (!priority && !room->water_face && !room->rigid_group && !room->bond) continue;
        unsigned at = 0;
        while (at < count && priorities[at] <= priority) ++at;
        if (at == DC_CAMERA_WORKSPACE_LIMIT) continue;
        if (count < DC_CAMERA_WORKSPACE_LIMIT) ++count;
        for (unsigned j = count - 1u; j > at; --j) {
            priorities[j] = priorities[j - 1u];
            selected[j] = selected[j - 1u];
        }
        priorities[at] = priority;
        selected[at] = room;
    }
    for (size_t i = 0; i <= view->sealed_capacity; ++i) {
        dc_camera_offscreen_t *room = i ? view->sealed_rooms + i - 1u : &view->camera_offscreen;
        if (room->phase == DC_CAMERA_OFFSCREEN_IDLE) continue;
        bool chosen = false, had_workspace = room->workspace != NULL;
        for (unsigned n = 0; n < count; ++n) chosen |= selected[n] == room;
        bool needed = camera_needs_chunk(view, room->coord);
        /* Overlap one returning room's write with the next room's load. */
        bool capacity = live < DC_CAMERA_WORKSPACE_LIMIT && (!needed || returning < 2u);
        if (!service_room(view, room, closing, chosen, capacity, err, cap)) return false;
        if (had_workspace != (room->workspace != NULL)) {
            if (had_workspace) { --live; returning -= needed; }
            else { ++live; returning += needed; }
        }
    }
    return true;
}
