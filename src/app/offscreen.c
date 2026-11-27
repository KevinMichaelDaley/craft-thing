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
#include "offscreen.h"

enum {
    OFFSCREEN_SIDE = 3u,
    OFFSCREEN_SLOTS = OFFSCREEN_SIDE * OFFSCREEN_SIDE,
    OFFSCREEN_STEP_LIMIT = 16u
};
static const double OFFSCREEN_MAX_STEP_SECONDS = .05;
_Static_assert(OFFSCREEN_SLOTS <= DC_GPU_CHUNK_SLOTS, "Offscreen atlas capacity");

struct dc_offscreen {
    dc_gpu_t *gpu;
    dc_chunk_coord_t origin;
    uint64_t generations[OFFSCREEN_SLOTS];
    dc_chunk_time_t time;
    bool failed;
};

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool page_at(const dc_offscreen_t *workspace, dc_chunk_coord_t coord, uint32_t *slot) {
    if (coord.x < workspace->origin.x || coord.y < workspace->origin.y) return false;
    uint64_t x = (uint64_t)coord.x - (uint64_t)workspace->origin.x;
    uint64_t y = (uint64_t)coord.y - (uint64_t)workspace->origin.y;
    if (x >= OFFSCREEN_SIDE || y >= OFFSCREEN_SIDE) return false;
    *slot = (uint32_t)(y * OFFSCREEN_SIDE + x);
    return true;
}

static bool configure_physics(dc_gpu_t *gpu, const dc_gpu_t *parent, char *err, uint32_t cap) {
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
    if (!source->time.known || !isfinite(source->time.seconds) || source->time.seconds < 0.)
        return error(err, cap, "Offscreen chunk has no confirmed time");
    if (workspace->time.known && workspace->time.seconds != source->time.seconds)
        return error(err, cap, "Offscreen workspace chunks have different completed times");
    uint32_t slot;
    if (!page_at(workspace, source->coord, &slot) || workspace->generations[slot])
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
    if (!workspace->time.known) return true;
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
    if (!workspace || workspace->failed || !out || !generation ||
        !page_at(workspace, coord, &slot) || workspace->generations[slot] != generation)
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
