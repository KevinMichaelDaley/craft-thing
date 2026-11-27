#include <math.h>

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas.h"
#include "ferrum/fire/thermal_gpu.h"
#include "../vulkan/gpu_internal.h"
#include "camera_offscreen.h"
#include "level_internal.h"
#include "level_offscreen.h"

enum { CAMERA_WORKSPACE_STEPS_PER_TICK = 2u, GAS_CHECKPOINT_VOLUME_WORD = 12u };

static bool same_coord(dc_chunk_coord_t a, dc_chunk_coord_t b) {
    return a.x == b.x && a.y == b.y;
}

static bool isolated_solvers(const dc_gpu_t *gpu) {
    return gpu && !gpu->body_count && !gpu->structural_graph_possible &&
        !fr_thermal_gpu_enabled(gpu) && !fr_combustion_gpu_enabled(gpu);
}

static bool cell_has_transport_stock(const dc_stream_result_t *snapshot, uint32_t index) {
    if (snapshot->chunk->cells[index].fluid_mass ||
        (snapshot->bulk && (snapshot->bulk->oil_fp[index] || snapshot->bulk->slime_fp[index])))
        return true;
    const fr_gas_checkpoint_chunk_t *gas = snapshot->gas;
    if (!gas) return false;
    if (gas->carrier_tail[index][0] || gas->carrier_tail[index][1]) return true;
    for (uint32_t species = 0; species < FR_GAS_SPECIES; ++species) {
        if (gas->cells[index][species]) return true;
    }
    return false;
}

static bool has_transport_stock(const dc_stream_result_t *snapshot) {
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        if (cell_has_transport_stock(snapshot, i)) return true;
    }
    return false;
}

static bool closed_edge_cell(const dc_stream_result_t *snapshot, uint32_t index) {
    const dc_cell_t *cell = snapshot->chunk->cells + index;
    return cell->material == DC_MATERIAL_STONE && (cell->flags & DC_CELL_LOADED_TERRAIN) &&
        !(cell->flags & DC_CELL_RELEASED_TERRAIN) &&
        (!snapshot->gas || !snapshot->gas->cells[index][GAS_CHECKPOINT_VOLUME_WORD]) &&
        !cell_has_transport_stock(snapshot, index);
}

bool dc_camera_offscreen_isolated(const dc_level_view_t *view, const dc_stream_result_t *snapshot) {
    if (!snapshot->chunk || !snapshot->time.known || !isfinite(snapshot->time.seconds) ||
        snapshot->time.seconds < 0. || !isolated_solvers(view->gpu) ||
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

void dc_camera_offscreen_offer(dc_level_view_t *view, const dc_stream_result_t *snapshot,
    dc_chunk_owner_t owner, uint64_t write_generation) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (camera->phase != DC_CAMERA_OFFSCREEN_IDLE || view->closing ||
        dc_level_chunk_visible(view, snapshot->coord) || !dc_chunk_owner_valid(owner) ||
        !dc_camera_offscreen_isolated(view, snapshot) || !has_transport_stock(snapshot)) return;
    *camera = (dc_camera_offscreen_t){.coord = snapshot->coord, .owner = owner,
        .time = snapshot->time, .generation = write_generation, .phase = DC_CAMERA_OFFSCREEN_SAVING};
}

void dc_camera_offscreen_saved(dc_level_view_t *view, const dc_stream_result_t *result) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (camera->phase != DC_CAMERA_OFFSCREEN_SAVING || camera->generation != result->generation ||
        !same_coord(camera->coord, result->coord)) return;
    if (result->kind == DC_STREAM_FAILED) {
        *camera = (dc_camera_offscreen_t){0};
    } else if (result->kind == DC_STREAM_SAVED &&
               dc_chunk_owner_equal(result->replaces_owner, camera->owner)) {
        camera->phase = DC_CAMERA_OFFSCREEN_STORED;
    }
}

void dc_camera_offscreen_discard(dc_level_view_t *view) {
    dc_offscreen_destroy(view->camera_offscreen.workspace);
    view->camera_offscreen = (dc_camera_offscreen_t){0};
}

static bool camera_needs_chunk(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return dc_level_chunk_visible(view, coord) ||
        (view->transfer_pending && same_coord(view->transfer_to, coord));
}

bool dc_camera_offscreen_service(dc_level_view_t *view, bool closing, char *err, uint32_t cap) {
    dc_camera_offscreen_t *camera = &view->camera_offscreen;
    if (camera->phase == DC_CAMERA_OFFSCREEN_IDLE || camera->phase == DC_CAMERA_OFFSCREEN_SAVING)
        return true;
    bool needed = camera_needs_chunk(view, camera->coord);
    bool independent = isolated_solvers(view->gpu);
    if (camera->phase == DC_CAMERA_OFFSCREEN_STORED) {
        if (closing || !independent || dc_chunk_table_find(&view->table, camera->coord, NULL) ||
            (needed && camera->time.seconds >= view->world_seconds)) {
            dc_camera_offscreen_discard(view);
            return true;
        }
        camera->workspace = dc_offscreen_create(view->gpu, camera->coord, NULL, 0);
        if (!camera->workspace || !dc_level_offscreen_begin_disk(view, camera->workspace,
            camera->coord, camera->owner, camera->time, &camera->generation, NULL, 0)) {
            dc_camera_offscreen_discard(view);
            return true;
        }
        camera->phase = DC_CAMERA_OFFSCREEN_LEASED;
    }

    dc_chunk_time_t time;
    bool writing;
    if (!dc_level_offscreen_progress(view, camera->coord, camera->generation, &time, &writing)) {
        dc_camera_offscreen_discard(view);
        return true;
    }
    if (!time.known || writing) return true;
    if (!closing && independent && !dc_offscreen_advance(camera->workspace,
        view->world_seconds, CAMERA_WORKSPACE_STEPS_PER_TICK, err, cap)) {
        dc_level_offscreen_cancel(view, camera->coord, camera->generation, NULL, 0);
        dc_camera_offscreen_discard(view);
        return false;
    }
    dc_level_offscreen_progress(view, camera->coord, camera->generation, &time, &writing);
    if (closing || !independent || (needed && time.seconds >= view->world_seconds))
        return dc_level_offscreen_publish(view, camera->coord, camera->generation, err, cap);
    return true;
}
