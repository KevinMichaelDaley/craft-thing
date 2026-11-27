#include <math.h>
#include <stdlib.h>

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
        dc_camera_offscreen_t *spare = NULL;
        for (unsigned i = 0; i < DC_CAMERA_WORKSPACE_LIMIT - 1u; ++i) {
            if (view->sealed_rooms[i].phase == DC_CAMERA_OFFSCREEN_IDLE) {
                spare = view->sealed_rooms + i;
                break;
            }
        }
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
    for (unsigned i = 0; i < DC_CAMERA_WORKSPACE_LIMIT - 1u; ++i)
        saved_room(view->sealed_rooms + i, result);
}

void dc_camera_offscreen_release(dc_camera_offscreen_t *camera) {
    free(camera->bond);
    dc_offscreen_destroy(camera->workspace);
    *camera = (dc_camera_offscreen_t){0};
}

void dc_camera_offscreen_discard(dc_level_view_t *view) {
    dc_camera_offscreen_release(&view->camera_offscreen);
    for (unsigned i = 0; i < DC_CAMERA_WORKSPACE_LIMIT - 1u; ++i)
        dc_camera_offscreen_release(view->sealed_rooms + i);
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

static bool service_room(dc_level_view_t *view, dc_camera_offscreen_t *camera, bool closing,
    char *err, uint32_t cap) {
    if (camera->bond) return dc_camera_bond_service(view, closing, err, cap);
    if (camera->phase == DC_CAMERA_OFFSCREEN_IDLE || camera->phase == DC_CAMERA_OFFSCREEN_SAVING)
        return true;
    bool needed = camera_needs_chunk(view, camera->coord);
    bool independent = dc_gpu_chunk_physics_isolated(view->gpu) || dc_offscreen_body_island(view->gpu, camera->coord);
    bool coupled = camera->water_face != 0u;
    bool required = coupled || camera->rigid_group;
    bool attached = coupled && face_in_view(view, camera->coord, camera->water_face);
    if (camera->phase == DC_CAMERA_OFFSCREEN_STORED) {
        if (closing || (coupled ? !attached : !independent) || dc_chunk_table_find(&view->table, camera->coord, NULL) ||
            (needed && camera->time.seconds >= view->world_seconds)) {
            dc_camera_offscreen_release(camera);
            return true;
        }
        camera->workspace = dc_offscreen_create(view->gpu, camera->coord, required ? err : NULL, required ? cap : 0u);
        if (!camera->workspace || !dc_level_offscreen_begin_disk(view, camera->workspace,
            camera->coord, camera->owner, camera->time, &camera->generation, required ? err : NULL, required ? cap : 0u)) {
            if (required) {
                dc_camera_offscreen_retry(camera);
                return false;
            }
            dc_camera_offscreen_release(camera);
            return true;
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
    if (!time.known || writing) return true;
    if (coupled || camera->rigid_group) {
        if (closing || needed || (coupled && !attached))
            return dc_level_offscreen_publish(view, camera->coord, camera->generation, err, cap);
        return true;
    }
    if (!closing && independent && !dc_offscreen_advance(camera->workspace,
        view->world_seconds, CAMERA_WORKSPACE_STEPS_PER_TICK, err, cap)) {
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
    if (!service_room(view, &view->camera_offscreen, closing, err, cap)) return false;
    for (unsigned i = 0; i < DC_CAMERA_WORKSPACE_LIMIT - 1u; ++i)
        if (!service_room(view, view->sealed_rooms + i, closing, err, cap)) return false;
    return true;
}
