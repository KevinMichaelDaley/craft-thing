#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "../vulkan/gas_gpu_state.h"
#include "../vulkan/reactive_gpu_internal.h"
#include "../vulkan/structural_store_fluids.h"
#include "../vulkan/thermal_gpu_internal.h"
#include "level_internal.h"
#include "level_offscreen.h"
#include "offscreen.h"
#include "offscreen_internal.h"

enum { OFFSCREEN_STEP_LIMIT = 16u };
static const double OFFSCREEN_MAX_STEP_SECONDS = .05;
_Static_assert(OFFSCREEN_SLOTS <= DC_GPU_CHUNK_SLOTS, "Offscreen atlas capacity");

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

bool dc_offscreen_page_at(const dc_offscreen_t *workspace, dc_chunk_coord_t coord, uint32_t *slot) {
    if (coord.x < workspace->origin.x || coord.y < workspace->origin.y) return false;
    uint64_t x = (uint64_t)coord.x - (uint64_t)workspace->origin.x;
    uint64_t y = (uint64_t)coord.y - (uint64_t)workspace->origin.y;
    if (x >= OFFSCREEN_SIDE || y >= OFFSCREEN_SIDE) return false;
    *slot = (uint32_t)(y * OFFSCREEN_SIDE + x);
    return true;
}

static bool configure_physics(dc_gpu_t *gpu, const dc_gpu_t *parent, char *err, uint32_t cap) {
    if (!dc_gpu_set_fluid_interval(gpu, parent->fluid_interval) ||
        !dc_gpu_set_marker_correction(gpu, parent->marker_correction))
        return error(err, cap, "Cannot configure workspace water simulation");
    if (fr_thermal_gpu_enabled(parent) && !fr_thermal_gpu_enable(gpu, err, cap)) return false;
    const struct fr_gas_gpu *gas = parent->gas;
    if (gas) {
        const float *parameters = gas->parameters.mapped;
        if (!fr_gas_gpu_enable(gpu, &gas->model, gas->density, gas->temperature, err, cap) ||
            !fr_gas_gpu_set_pressure_policy(gpu, gas->policy, err, cap) ||
            !fr_gas_gpu_set_gravity(gpu, parameters[FR_GAS_GRAVITY_WORD], err, cap) ||
            !fr_gas_gpu_set_wind(gpu, parameters[FR_GAS_WIND_WORD], err, cap)) return false;
    }
    return !fr_combustion_gpu_enabled(parent) || fr_combustion_gpu_enable(gpu, err, cap);
}

dc_offscreen_t *dc_offscreen_create(dc_gpu_t *parent, dc_chunk_coord_t origin,
                                    char *err, uint32_t cap) {
    if (!parent || origin.x > INT64_MAX - (OFFSCREEN_SIDE - 1u) ||
        origin.y > INT64_MAX - (OFFSCREEN_SIDE - 1u)) {
        error(err, cap, "Invalid offscreen workspace bounds");
        return NULL;
    }
    dc_offscreen_t *workspace = calloc(1, sizeof(*workspace));
    if (!workspace) {
        error(err, cap, "Cannot allocate offscreen workspace");
        return NULL;
    }
    workspace->origin = origin;
    if (!dc_gpu_create_shared(&workspace->gpu, parent, OFFSCREEN_SIDE * DC_CHUNK_SIDE,
                              OFFSCREEN_SIDE * DC_CHUNK_SIDE, "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_set_body_origin(workspace->gpu, origin, err, cap) ||
        !configure_physics(workspace->gpu, parent, err, cap)) {
        dc_offscreen_destroy(workspace);
        return NULL;
    }
    return workspace;
}

void dc_offscreen_destroy(dc_offscreen_t *workspace) {
    if (!workspace) return;
    dc_gpu_destroy(workspace->gpu);
    free(workspace);
}

bool dc_offscreen_admit(dc_offscreen_t *workspace, const dc_stream_result_t *source,
                        uint64_t generation, char *err, uint32_t cap) {
    if (!workspace || workspace->failed || !source || source->kind != DC_STREAM_LOADED ||
        !source->chunk || !generation || source->generation != generation ||
        source->coord.x != source->chunk->coord.x || source->coord.y != source->chunk->coord.y)
        return error(err, cap, "Invalid offscreen chunk owner");
    if (dc_gpu_liquid_owner_held(workspace->gpu))
        return error(err, cap, "Water cycle holds the offscreen workspace");
    if (!source->time.known || !isfinite(source->time.seconds) || source->time.seconds < 0.)
        return error(err, cap, "Offscreen chunk has no confirmed time");
    if (workspace->time.known && workspace->time.seconds != source->time.seconds)
        return error(err, cap, "Offscreen workspace chunks have different completed times");
    uint32_t slot;
    if (!dc_offscreen_page_at(workspace, source->coord, &slot) || workspace->generations[slot])
        return error(err, cap, "Offscreen chunk page is unavailable");
    dc_gpu_t *gpu = workspace->gpu;
    if ((source->gas != NULL) != (gpu->gas != NULL) ||
        (source->water_excess && !fr_thermal_gpu_enabled(gpu)))
        return error(err, cap, "Offscreen snapshot channels do not match the configured physics");

    if (!dc_gpu_upload_chunk_exact(gpu, slot, source->chunk, false, err, cap) ||
        (source->colors && !fr_renderer_upload_chunk_colors(gpu, slot, source->colors, err, cap)) ||
        !fr_gpu_reactive_import_chunk(gpu, slot, source->chunk, source->reactive, err, cap) ||
        (source->water_excess && !fr_thermal_gpu_import_water_chunk(gpu, slot, source->chunk, source->water_excess, err, cap)) ||
        (source->bulk && !fr_bulk_phase_gpu_import_chunk(gpu, slot, source->chunk, source->bulk, err, cap)) ||
        (source->gas && !fr_gas_gpu_import_checkpoint(gpu, slot, source->gas, err, cap)) ||
        !dc_gpu_set_page(gpu, slot % OFFSCREEN_SIDE, slot / OFFSCREEN_SIDE, slot, err, cap)) {
        workspace->failed = true;
        return false;
    }
    workspace->time = source->time;
    workspace->generations[slot] = generation;
    return true;
}

bool dc_offscreen_advance(dc_offscreen_t *workspace, double world_seconds, uint32_t step_budget,
                          char *err, uint32_t cap) {
    if (!workspace || workspace->failed || !isfinite(world_seconds) || world_seconds < 0.)
        return error(err, cap, "Invalid offscreen advance or failed workspace");
    if (!workspace->time.known || workspace->held_writes || workspace->water_coupled ||
        dc_gpu_liquid_owner_held(workspace->gpu))
        return true;
    if (step_budget > OFFSCREEN_STEP_LIMIT) step_budget = OFFSCREEN_STEP_LIMIT;
    for (uint32_t i = 0; i < step_budget && workspace->time.seconds < world_seconds; ++i) {
        double interval = fmin(world_seconds - workspace->time.seconds, OFFSCREEN_MAX_STEP_SECONDS);
        float seconds = (float)interval;
        if ((double)seconds > interval) seconds = nextafterf(seconds, 0.f);
        double completed = workspace->time.seconds + (double)seconds;
        if (completed <= workspace->time.seconds || !dc_gpu_set_tick_seconds(workspace->gpu, seconds))
            return error(err, cap, "Offscreen interval cannot advance the confirmed clock");
        if (!dc_gpu_tick_step(workspace->gpu, err, cap)) {
            /* Submission may already have changed one or more physics channels. */
            workspace->failed = true;
            return false;
        }
        workspace->time.seconds = completed;
    }
    return true;
}

static bool snapshot_channels(dc_gpu_t *gpu, uint32_t slot, dc_stream_result_t *result,
                               char *err, uint32_t cap) {
    result->chunk = malloc(sizeof(*result->chunk));
    result->colors = malloc(sizeof(*result->colors));
    result->bulk = malloc(sizeof(*result->bulk));
    result->reactive = calloc(1, sizeof(*result->reactive));
    if (fr_thermal_gpu_enabled(gpu)) result->water_excess = malloc(sizeof(float) * DC_CHUNK_CELLS);
    if (gpu->gas) result->gas = malloc(sizeof(*result->gas));
    if (!result->chunk || !result->colors || !result->bulk || !result->reactive ||
        (fr_thermal_gpu_enabled(gpu) && !result->water_excess) || (gpu->gas && !result->gas))
        return error(err, cap, "Cannot allocate offscreen snapshot channels");
    result->chunk->coord = result->coord;
    return dc_gpu_download_chunk(gpu, slot, result->chunk, err, cap) &&
           fr_renderer_download_chunk_colors(gpu, slot, result->colors, err, cap) &&
           fr_gpu_reactive_export_chunk(gpu, slot, result->chunk, result->reactive, err, cap) &&
           fr_bulk_phase_gpu_export_chunk(gpu, slot, result->bulk, err, cap) &&
           (!result->water_excess || fr_thermal_gpu_read_water_chunk(gpu, slot, result->water_excess, DC_CHUNK_CELLS, err, cap)) &&
           (!result->gas || fr_gas_gpu_export_checkpoint(gpu, slot, result->gas, err, cap));
}

bool dc_offscreen_snapshot(dc_offscreen_t *workspace, dc_chunk_coord_t coord,
                           uint64_t generation, dc_stream_result_t *out,
                           char *err, uint32_t cap) {
    uint32_t slot;
    if (!workspace || workspace->failed || !out || !generation || dc_gpu_liquid_owner_held(workspace->gpu) ||
        !dc_offscreen_page_at(workspace, coord, &slot) || workspace->generations[slot] != generation)
        return error(err, cap, "Offscreen snapshot owner is unavailable");
    dc_stream_result_t result = {.kind = DC_STREAM_LOADED, .coord = coord,
        .generation = generation, .time = workspace->time};
    if (!snapshot_channels(workspace->gpu, slot, &result, err, cap)) {
        dc_stream_result_release(&result);
        return false;
    }
    if (result.reactive->count == 0u && result.reactive->next_id == 1u) {
        free(result.reactive);
        result.reactive = NULL;
    }
    *out = result;
    return true;
}

static dc_level_offscreen_handoff_t **find_handoff(dc_level_view_t *view, dc_chunk_coord_t coord) {
    dc_level_offscreen_handoff_t **link = &view->offscreen_handoffs;
    while (*link && ((*link)->coord.x != coord.x || (*link)->coord.y != coord.y))
        link = &(*link)->next;
    return link;
}

dc_level_offscreen_handoff_t *dc_level_offscreen_lookup(dc_level_view_t *view, dc_chunk_coord_t coord) {
    return view ? *find_handoff(view, coord) : NULL;
}

static void remove_handoff(dc_level_offscreen_handoff_t **link) {
    dc_level_offscreen_handoff_t *old = *link;
    *link = old->next;
    if (old->write_generation) --old->workspace->held_writes;
    free(old);
}

static uint32_t pending_source(const dc_gpu_t *gpu, dc_chunk_coord_t coord) {
    uint32_t index = dc_gpu_pending_chunk_index(gpu, coord);
    if (index == UINT32_MAX || !gpu->checkpoint_owners || !gpu->checkpoint_times ||
        !gpu->checkpoint_fluids || !gpu->checkpoint_fluids[index].known ||
        !gpu->checkpoint_times[index].known || !dc_chunk_owner_valid(gpu->checkpoint_owners[index]))
        return UINT32_MAX;
    return index;
}

static bool retire_page(dc_level_offscreen_handoff_t *handoff, char *err, uint32_t cap) {
    dc_offscreen_t *workspace = handoff->workspace;
    uint32_t slot;
    if (!dc_offscreen_page_at(workspace, handoff->coord, &slot) || workspace->generations[slot] != handoff->generation)
        return error(err, cap, "Offscreen retirement owner changed");
    if (!dc_gpu_set_page(workspace->gpu, slot % OFFSCREEN_SIDE, slot / OFFSCREEN_SIDE,
                        UINT32_MAX, err, cap) ||
        !fr_gpu_reactive_clear_chunk(workspace->gpu, slot, err, cap)) {
        workspace->failed = true;
        return false;
    }
    workspace->generations[slot] = 0;
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i) {
        if (workspace->generations[i]) return true;
    }
    workspace->time = (dc_chunk_time_t){0};
    workspace->water_coupled = false;
    return true;
}

bool dc_level_offscreen_owns(dc_level_view_t *view, dc_chunk_coord_t coord) {
    return *find_handoff(view, coord) != NULL;
}

static bool can_reserve(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, uint64_t *generation) {
    uint32_t slot;
    return view && view->gpu && view->stream && !view->closing && !view->liquid_seam_failed &&
        workspace && !workspace->failed &&
        generation && view->table.next_generation && view->table.next_generation != UINT64_MAX &&
        dc_offscreen_page_at(workspace, coord, &slot) && !workspace->generations[slot] &&
        !dc_chunk_table_find(&view->table, coord, NULL) && !dc_level_offscreen_owns(view, coord);
}

static bool begin_handoff(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, dc_chunk_owner_t owner, dc_chunk_time_t disk_time,
    uint64_t *generation, char *err, uint32_t cap) {
    dc_level_offscreen_handoff_t *handoff = calloc(1, sizeof(*handoff));
    if (!handoff) return error(err, cap, "Cannot allocate offscreen handoff");
    handoff->generation = view->table.next_generation++;
    if (!dc_stream_request_load(view->stream, coord, handoff->generation)) {
        free(handoff);
        return error(err, cap, "Cannot queue offscreen source load");
    }
    handoff->workspace = workspace;
    handoff->coord = coord;
    handoff->owner = owner;
    handoff->disk_source = disk_time.known;
    handoff->source_time = disk_time;
    handoff->next = view->offscreen_handoffs;
    view->offscreen_handoffs = handoff;
    *generation = handoff->generation;
    return true;
}

bool dc_level_offscreen_begin(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, uint64_t *generation, char *err, uint32_t cap) {
    if (!can_reserve(view, workspace, coord, generation))
        return error(err, cap, "Offscreen chunk cannot be reserved");
    uint32_t index = pending_source(view->gpu, coord);
    if (index == UINT32_MAX) return error(err, cap, "Offscreen chunk has no versioned structure owner");
    return begin_handoff(view, workspace, coord, view->gpu->checkpoint_owners[index],
                         (dc_chunk_time_t){0}, generation, err, cap);
}

bool dc_level_offscreen_begin_disk(dc_level_view_t *view, dc_offscreen_t *workspace,
    dc_chunk_coord_t coord, dc_chunk_owner_t owner, dc_chunk_time_t time,
    uint64_t *generation, char *err, uint32_t cap) {
    if (!can_reserve(view, workspace, coord, generation) || !dc_chunk_owner_valid(owner) ||
        !time.known || !isfinite(time.seconds) || time.seconds < 0.)
        return error(err, cap, "Offscreen eviction cannot be reserved");
    return begin_handoff(view, workspace, coord, owner, time, generation, err, cap);
}

bool dc_level_offscreen_progress(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, dc_chunk_time_t *time, bool *writing) {
    dc_level_offscreen_handoff_t *handoff = *find_handoff(view, coord);
    if (!handoff || handoff->generation != generation) return false;
    *time = handoff->admitted ? handoff->workspace->time : (dc_chunk_time_t){0};
    *writing = handoff->write_generation != 0;
    return true;
}

bool dc_level_offscreen_publish(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, char *err, uint32_t cap) {
    dc_level_offscreen_handoff_t *handoff = view ? *find_handoff(view, coord) : NULL;
    if (!handoff || !handoff->admitted || handoff->generation != generation ||
        handoff->write_generation || view->closing || view->liquid_seam_failed ||
        view->table.next_generation == UINT64_MAX)
        return error(err, cap, "Offscreen owner cannot publish");
    if (!handoff->disk_source) {
        uint32_t index = pending_source(view->gpu, coord);
        if (index == UINT32_MAX || !dc_chunk_owner_equal(view->gpu->checkpoint_owners[index], handoff->owner))
            return error(err, cap, "Offscreen replacement parent changed");
    }
    dc_stream_result_t snapshot = {0};
    if (!dc_offscreen_snapshot(handoff->workspace, coord, generation, &snapshot, err, cap)) return false;
    bool okay = snapshot.time.seconds >= handoff->source_time.seconds &&
                snapshot.time.seconds <= view->world_seconds;
    if (!okay) error(err, cap, "Offscreen replacement time is outside the world clock");
    if (okay && view->save) okay = dc_world_save_store_clock(view->save, view->world_seconds, err, cap);
    if (okay) {
        uint64_t write = view->table.next_generation++;
        if (dc_stream_request_save_takeover(view->stream, &snapshot, handoff->owner, write)) {
            handoff->write_generation = write;
            ++handoff->workspace->held_writes;
        } else {
            okay = error(err, cap, "Cannot queue offscreen replacement write");
        }
    }
    dc_stream_result_release(&snapshot);
    return okay;
}

bool dc_level_offscreen_cancel(dc_level_view_t *view, dc_chunk_coord_t coord,
    uint64_t generation, char *err, uint32_t cap) {
    if (!view) return false;
    dc_level_offscreen_handoff_t **link = find_handoff(view, coord);
    dc_level_offscreen_handoff_t *handoff = *link;
    if (!handoff || handoff->generation != generation || handoff->write_generation || handoff->liquid_paired ||
        handoff->owns_bodies ||
        dc_gpu_liquid_owner_held(handoff->workspace->gpu))
        return error(err, cap, "Offscreen owner cannot be canceled");
    bool okay = !handoff->admitted || handoff->workspace->failed || retire_page(handoff, err, cap);
    remove_handoff(link);
    return okay;
}

static bool accept_source(dc_level_view_t *view, dc_level_offscreen_handoff_t *handoff,
    const dc_stream_result_t *result, char *err, uint32_t cap) {
    if (result->kind == DC_STREAM_FAILED) return error(err, cap, "Offscreen source load failed");
    if (handoff->disk_source) {
        const dc_camera_offscreen_t *camera = &view->camera_offscreen;
        bool coupled = camera->water_face && camera->workspace == handoff->workspace;
        if (!dc_chunk_owner_equal(result->replaces_owner, handoff->owner) || !result->time.known ||
            result->time.seconds < handoff->source_time.seconds ||
            (coupled ? result->time.seconds != view->world_seconds ||
                dc_camera_offscreen_water_face(view, result) != camera->water_face :
                !dc_camera_offscreen_isolated(view, result)))
            return error(err, cap, "Offscreen eviction source changed");
        if (!dc_offscreen_admit(handoff->workspace, result, handoff->generation, err, cap)) return false;
        if (!coupled && dc_offscreen_body_island(view->gpu, result->coord)) {
            dc_gpu_t *destination = handoff->workspace->gpu;
            if (!dc_level_view_stage_world_save(view, err, cap) ||
                !dc_gpu_set_rigid_solver(destination, view->gpu->rigid_solver_enabled) ||
                !dc_offscreen_move_bodies(view->gpu, destination, result->coord, err, cap)) return false;
            destination->rigid_step_budget = view->gpu->rigid_step_budget;
            handoff->owns_bodies = true;
        }
        handoff->source_time = result->time;
        handoff->admitted = true;
        return true;
    }
    dc_gpu_t *gpu = view->gpu;
    uint32_t index = pending_source(gpu, handoff->coord);
    if (index == UINT32_MAX || !dc_chunk_owner_equal(gpu->checkpoint_owners[index], handoff->owner))
        return error(err, cap, "Offscreen source owner changed");
    dc_checkpoint_fluid_t *fluid = gpu->checkpoint_fluids + index;
    dc_stream_result_t checkpoint = {.kind = DC_STREAM_LOADED, .coord = handoff->coord,
        .generation = handoff->generation, .chunk = gpu->checkpoint_chunks + index,
        .time = gpu->checkpoint_times[index], .gas = fluid->gas, .bulk = fluid->bulk,
        .colors = gpu->checkpoint_colors ? gpu->checkpoint_colors + index : NULL,
        .reactive = gpu->checkpoint_reactive ? gpu->checkpoint_reactive + index : NULL,
        .water_excess = fr_thermal_checkpoint_pending_water(gpu, index)};
    const dc_stream_result_t *source = &checkpoint;
    if (dc_chunk_owner_equal(result->replaces_owner, handoff->owner) && result->time.known &&
        result->time.seconds >= checkpoint.time.seconds) source = result;
    if (!dc_offscreen_admit(handoff->workspace, source, handoff->generation, err, cap)) return false;
    handoff->source_time = source->time;
    handoff->admitted = true;
    return true;
}

dc_level_offscreen_result_t dc_level_offscreen_result(dc_level_view_t *view,
    const dc_stream_result_t *result, char *err, uint32_t cap) {
    dc_level_offscreen_handoff_t **link = find_handoff(view, result->coord);
    dc_level_offscreen_handoff_t *handoff = *link;
    if (!handoff) return DC_LEVEL_OFFSCREEN_UNMATCHED;
    if (!handoff->admitted && handoff->generation == result->generation &&
        (result->kind == DC_STREAM_LOADED || result->kind == DC_STREAM_FAILED)) {
        if (view->closing) {
            remove_handoff(link);
            return DC_LEVEL_OFFSCREEN_HANDLED;
        }
        if (accept_source(view, handoff, result, err, cap)) return DC_LEVEL_OFFSCREEN_HANDLED;
        bool retry_camera = (view->camera_offscreen.water_face || view->camera_offscreen.rigid_group) &&
                            handoff->workspace == view->camera_offscreen.workspace;
        remove_handoff(link);
        if (retry_camera) dc_camera_offscreen_retry(&view->camera_offscreen);
        return DC_LEVEL_OFFSCREEN_FAILED;
    }
    if (!handoff->write_generation || handoff->write_generation != result->generation)
        return DC_LEVEL_OFFSCREEN_UNMATCHED;
    if (result->kind == DC_STREAM_FAILED) {
        --handoff->workspace->held_writes;
        handoff->write_generation = 0;
        error(err, cap, "Offscreen replacement write failed");
        return DC_LEVEL_OFFSCREEN_FAILED;
    }
    if (result->kind != DC_STREAM_SAVED || !dc_chunk_owner_equal(result->replaces_owner, handoff->owner))
        return DC_LEVEL_OFFSCREEN_UNMATCHED;
    if (handoff->owns_bodies) {
        if (!dc_offscreen_move_bodies(handoff->workspace->gpu, view->gpu, handoff->coord, err, cap)) {
            --handoff->workspace->held_writes;
            handoff->write_generation = 0;
            return DC_LEVEL_OFFSCREEN_FAILED;
        }
        handoff->owns_bodies = false;
        view->closing_structure_saved = false;
    }
    bool okay = retire_page(handoff, err, cap);
    remove_handoff(link);
    return okay ? DC_LEVEL_OFFSCREEN_HANDLED : DC_LEVEL_OFFSCREEN_FAILED;
}

bool dc_level_offscreen_writing(const dc_level_view_t *view) {
    for (const dc_level_offscreen_handoff_t *p = view->offscreen_handoffs; p; p = p->next) {
        if (p->write_generation) return true;
    }
    return false;
}

void dc_level_offscreen_discard(dc_level_view_t *view) {
    while (view->offscreen_handoffs) remove_handoff(&view->offscreen_handoffs);
}
