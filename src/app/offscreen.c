#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "offscreen.h"
#include "view_config.h"

typedef struct {
    dc_chunk_coord_t coord;
    bool occupied;
} offscreen_slot_t;

struct dc_offscreen {
    dc_gpu_t *gpu;
    dc_chunk_coord_t origin;
    offscreen_slot_t slots[DC_GPU_CHUNK_SLOTS];
    uint32_t count;
    double pending_seconds;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static bool same_coord(dc_chunk_coord_t a, dc_chunk_coord_t b) {
    return a.x == b.x && a.y == b.y;
}

dc_offscreen_t *dc_offscreen_create(dc_chunk_coord_t origin,
                                    char *err, uint32_t cap) {
    dc_offscreen_t *offscreen = calloc(1, sizeof(*offscreen));
    if (!offscreen) { error(err, cap, "Cannot allocate offscreen cache"); return NULL; }
    offscreen->origin = origin;
    if (!dc_gpu_create(&offscreen->gpu, SIM_WIDTH, SIM_HEIGHT,
                       "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_set_fluid_interval(offscreen->gpu, 1u) ||
        !dc_gpu_set_pressure_sweeps(offscreen->gpu, 8u)) {
        dc_offscreen_destroy(offscreen);
        return NULL;
    }
    return offscreen;
}

void dc_offscreen_destroy(dc_offscreen_t *offscreen) {
    if (!offscreen) return;
    dc_gpu_destroy(offscreen->gpu);
    free(offscreen);
}

bool dc_offscreen_contains(const dc_offscreen_t *offscreen, dc_chunk_coord_t coord) {
    if (!offscreen) return false;
    return coord.x >= offscreen->origin.x - HALO_CHUNKS &&
           coord.x < offscreen->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS &&
           coord.y >= offscreen->origin.y - HALO_CHUNKS &&
           coord.y < offscreen->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS;
}

bool dc_offscreen_can_capture(const dc_offscreen_t *offscreen,
                              dc_chunk_coord_t coord) {
    return dc_offscreen_contains(offscreen, coord) &&
           offscreen->count < DC_GPU_CHUNK_SLOTS &&
           !dc_offscreen_has(offscreen, coord);
}

bool dc_offscreen_has(const dc_offscreen_t *offscreen, dc_chunk_coord_t coord) {
    if (!offscreen) return false;
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i)
        if (offscreen->slots[i].occupied &&
            same_coord(offscreen->slots[i].coord, coord)) return true;
    return false;
}

void dc_offscreen_set_spring(dc_offscreen_t *offscreen, bool enabled) {
    if (!offscreen) return;
    dc_chunk_coord_t source = {2 * WORLD_SCALE, 0};
    if (!enabled || !dc_offscreen_has(offscreen, source)) {
        dc_gpu_set_tick_water_source(offscreen->gpu, false, 0u, 0u);
        return;
    }
    uint32_t x = (uint32_t)(source.x - offscreen->origin.x + HALO_CHUNKS) *
                 DC_CHUNK_SIDE;
    uint32_t y = (uint32_t)(-offscreen->origin.y + HALO_CHUNKS) *
                 DC_CHUNK_SIDE + 4u * WORLD_SCALE;
    dc_gpu_set_tick_water_source(offscreen->gpu, true, x, y);
}

bool dc_offscreen_capture(dc_offscreen_t *offscreen, const dc_chunk_t *chunk,
                          char *err, uint32_t cap) {
    if (!offscreen || !chunk || !dc_offscreen_contains(offscreen, chunk->coord) ||
        dc_offscreen_has(offscreen, chunk->coord))
        return error(err, cap, "Offscreen chunk is outside workspace or duplicated");
    uint32_t index = DC_GPU_CHUNK_SLOTS;
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i)
        if (!offscreen->slots[i].occupied) { index = i; break; }
    if (index == DC_GPU_CHUNK_SLOTS)
        return error(err, cap, "Offscreen GPU cache is full");
    uint32_t tile_x = (uint32_t)(chunk->coord.x - offscreen->origin.x + HALO_CHUNKS);
    uint32_t tile_y = (uint32_t)(chunk->coord.y - offscreen->origin.y + HALO_CHUNKS);
    if (!dc_gpu_upload_chunk(offscreen->gpu, index, chunk, err, cap) ||
        !dc_gpu_set_page(offscreen->gpu, tile_x, tile_y, index, err, cap))
        return false;
    offscreen->slots[index] = (offscreen_slot_t){ .coord = chunk->coord,
                                                  .occupied = true };
    ++offscreen->count;
    return true;
}

bool dc_offscreen_take(dc_offscreen_t *offscreen, dc_chunk_coord_t coord,
                       dc_chunk_t *chunk, char *err, uint32_t cap) {
    if (!offscreen || !chunk) return error(err, cap, "Invalid offscreen chunk take");
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i) {
        if (!offscreen->slots[i].occupied ||
            !same_coord(offscreen->slots[i].coord, coord)) continue;
        chunk->coord = coord;
        uint32_t tile_x = (uint32_t)(coord.x - offscreen->origin.x + HALO_CHUNKS);
        uint32_t tile_y = (uint32_t)(coord.y - offscreen->origin.y + HALO_CHUNKS);
        if (!dc_gpu_download_chunk(offscreen->gpu, i, chunk, err, cap) ||
            !dc_gpu_set_page(offscreen->gpu, tile_x, tile_y, UINT32_MAX,
                             err, cap)) return false;
        offscreen->slots[i].occupied = false;
        --offscreen->count;
        return true;
    }
    return error(err, cap, "Offscreen chunk is not cached");
}

uint32_t dc_offscreen_coord_band(dc_chunk_coord_t coord,
                                 dc_chunk_coord_t camera_origin) {
    uint64_t dx = coord.x < camera_origin.x ?
        (uint64_t)camera_origin.x - (uint64_t)coord.x :
        coord.x >= camera_origin.x + VIEW_CHUNKS_X ?
        (uint64_t)coord.x - (uint64_t)camera_origin.x - VIEW_CHUNKS_X + 1u : 0u;
    uint64_t dy = coord.y < camera_origin.y ?
        (uint64_t)camera_origin.y - (uint64_t)coord.y :
        coord.y >= camera_origin.y + VIEW_CHUNKS_Y ?
        (uint64_t)coord.y - (uint64_t)camera_origin.y - VIEW_CHUNKS_Y + 1u : 0u;
    if (dx <= VIEW_CHUNKS_X && dy <= VIEW_CHUNKS_Y) return 1u;
    if (dx <= 2u * VIEW_CHUNKS_X && dy <= 2u * VIEW_CHUNKS_Y) return 2u;
    if (dx <= 4u * VIEW_CHUNKS_X && dy <= 4u * VIEW_CHUNKS_Y) return 3u;
    return 5u;
}

uint32_t dc_offscreen_band(const dc_offscreen_t *offscreen,
                           dc_chunk_coord_t camera_origin) {
    if (!offscreen || !offscreen->count) return 5u;
    uint32_t closest = 5u;
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i) {
        if (!offscreen->slots[i].occupied) continue;
        uint32_t band = dc_offscreen_coord_band(offscreen->slots[i].coord,
                                                  camera_origin);
        if (band < closest) closest = band;
    }
    return closest;
}

bool dc_offscreen_update(dc_offscreen_t *offscreen, dc_chunk_coord_t camera_origin,
                         double elapsed_seconds, bool catch_up,
                         char *err, uint32_t cap) {
    if (!offscreen) return true;
    uint32_t band = dc_offscreen_band(offscreen, camera_origin);
    if (band == 5u) { offscreen->pending_seconds = 0.0; return true; }
    offscreen->pending_seconds += elapsed_seconds;
    const uint32_t cadence = band == 1u ? 4u : band == 2u ? 12u : 24u;
    const double period = (double)cadence / 60.0;
    while (offscreen->pending_seconds + 1e-7 >= period ||
           (catch_up && offscreen->pending_seconds > 1e-7)) {
        double advance = offscreen->pending_seconds < period ?
                         offscreen->pending_seconds : period;
        uint32_t substeps = (uint32_t)(advance * 20.0 + 0.999999);
        if (!substeps) substeps = 1u;
        float substep_seconds = (float)(advance / substeps);
        for (uint32_t step = 0; step < substeps; ++step)
            if (!dc_gpu_set_tick_seconds(offscreen->gpu, substep_seconds) ||
                !dc_gpu_tick_step(offscreen->gpu, err, cap)) return false;
        offscreen->pending_seconds -= advance;
    }
    return true;
}

bool dc_offscreen_flush(dc_offscreen_t *offscreen, dc_offscreen_save_fn save,
                        void *context, char *err, uint32_t cap) {
    if (!offscreen || !save) return error(err, cap, "Invalid offscreen flush");
    dc_chunk_t *chunk = malloc(sizeof(*chunk));
    if (!chunk) return error(err, cap, "Cannot allocate offscreen save chunk");
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i) {
        if (!offscreen->slots[i].occupied) continue;
        chunk->coord = offscreen->slots[i].coord;
        if (!dc_gpu_download_chunk(offscreen->gpu, i, chunk, err, cap) ||
            !save(context, chunk, err, cap)) { free(chunk); return false; }
        offscreen->slots[i].occupied = false;
        --offscreen->count;
    }
    free(chunk);
    return true;
}
