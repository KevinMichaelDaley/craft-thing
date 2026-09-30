#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "offscreen.h"
#include "view_config.h"

enum {
    OFFSCREEN_TILES_X = SIM_CHUNKS_X < 3u ? SIM_CHUNKS_X : 3u,
    OFFSCREEN_TILES_Y = SIM_CHUNKS_Y < 3u ? SIM_CHUNKS_Y : 3u,
    OFFSCREEN_SLOTS = OFFSCREEN_TILES_X * OFFSCREEN_TILES_Y,
    OFFSCREEN_MAX_BATCH_STEPS = 16u
};

static const float OFFSCREEN_FLUID_RETAINED_PER_TICK = 0.92f;

_Static_assert(OFFSCREEN_SLOTS <= DC_GPU_CHUNK_SLOTS,
               "Offscreen workspace exceeds GPU chunk capacity");

typedef struct {
    dc_chunk_coord_t coord;
    bool occupied;
} offscreen_slot_t;

struct dc_offscreen {
    dc_gpu_t *gpu;
    dc_chunk_coord_t origin;
    offscreen_slot_t slots[OFFSCREEN_SLOTS];
    uint32_t count;
    double pending_seconds;
    float last_advance_ticks;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static bool same_coord(dc_chunk_coord_t a, dc_chunk_coord_t b) {
    return a.x == b.x && a.y == b.y;
}

dc_chunk_coord_t dc_offscreen_frontier_origin(dc_chunk_coord_t coord,
                                              dc_chunk_coord_t camera_origin) {
    const int64_t left_span = OFFSCREEN_TILES_X - HALO_CHUNKS - 1u;
    const int64_t top_span = OFFSCREEN_TILES_Y - HALO_CHUNKS - 1u;
    if (coord.x < camera_origin.x && coord.x >= INT64_MIN + left_span)
        coord.x -= left_span;
    if (coord.y < camera_origin.y && coord.y >= INT64_MIN + top_span)
        coord.y -= top_span;
    return coord;
}

dc_offscreen_t *dc_offscreen_create(dc_gpu_t *parent, dc_chunk_coord_t origin,
                                    char *err, uint32_t cap) {
    dc_offscreen_t *offscreen = calloc(1, sizeof(*offscreen));
    if (!offscreen) { error(err, cap, "Cannot allocate offscreen cache"); return NULL; }
    offscreen->origin = origin;
    if (!dc_gpu_create_shared(&offscreen->gpu, parent,
                       OFFSCREEN_TILES_X * DC_CHUNK_SIDE,
                       OFFSCREEN_TILES_Y * DC_CHUNK_SIDE,
                       "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_set_fluid_interval(offscreen->gpu, 1u) ||
        !dc_gpu_set_fluid_velocity_damping(offscreen->gpu,
                                           OFFSCREEN_FLUID_RETAINED_PER_TICK) ||
        !dc_gpu_set_pressure_sweeps(offscreen->gpu, 8u) ||
        !dc_gpu_set_tick_water_source_radius(offscreen->gpu, SPRING_RADIUS) ||
        !dc_gpu_set_tick_water_source_rate(offscreen->gpu,
                                            SPRING_RATE_QUARTERS)) {
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
    uint64_t dx = coord.x >= offscreen->origin.x ?
        (uint64_t)coord.x - (uint64_t)offscreen->origin.x :
        (uint64_t)offscreen->origin.x - (uint64_t)coord.x;
    uint64_t dy = coord.y >= offscreen->origin.y ?
        (uint64_t)coord.y - (uint64_t)offscreen->origin.y :
        (uint64_t)offscreen->origin.y - (uint64_t)coord.y;
    return (coord.x >= offscreen->origin.x ?
            dx < OFFSCREEN_TILES_X - HALO_CHUNKS : dx <= HALO_CHUNKS) &&
           (coord.y >= offscreen->origin.y ?
            dy < OFFSCREEN_TILES_Y - HALO_CHUNKS : dy <= HALO_CHUNKS);
}

bool dc_offscreen_can_capture(const dc_offscreen_t *offscreen,
                              dc_chunk_coord_t coord) {
    return dc_offscreen_contains(offscreen, coord) &&
           offscreen->count < OFFSCREEN_SLOTS &&
           !dc_offscreen_has(offscreen, coord);
}

bool dc_offscreen_has(const dc_offscreen_t *offscreen, dc_chunk_coord_t coord) {
    if (!offscreen) return false;
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i)
        if (offscreen->slots[i].occupied &&
            same_coord(offscreen->slots[i].coord, coord)) return true;
    return false;
}

bool dc_offscreen_slot(const dc_offscreen_t *offscreen, uint32_t slot,
                       dc_chunk_coord_t *coord, uint32_t *tile_x,
                       uint32_t *tile_y) {
    if (!offscreen || slot >= OFFSCREEN_SLOTS ||
        !offscreen->slots[slot].occupied || !coord || !tile_x || !tile_y)
        return false;
    *coord = offscreen->slots[slot].coord;
    *tile_x = (uint32_t)(coord->x - offscreen->origin.x + HALO_CHUNKS);
    *tile_y = (uint32_t)(coord->y - offscreen->origin.y + HALO_CHUNKS);
    return true;
}

uint32_t dc_offscreen_slot_capacity(const dc_offscreen_t *offscreen) {
    return offscreen ? OFFSCREEN_SLOTS : 0u;
}

dc_gpu_t *dc_offscreen_gpu(dc_offscreen_t *offscreen) {
    return offscreen ? offscreen->gpu : NULL;
}

float dc_offscreen_last_advance_ticks(const dc_offscreen_t *offscreen) {
    return offscreen ? offscreen->last_advance_ticks : 0.0f;
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
    uint32_t index = OFFSCREEN_SLOTS;
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i)
        if (!offscreen->slots[i].occupied) { index = i; break; }
    if (index == OFFSCREEN_SLOTS)
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
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i) {
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
    uint64_t rx = (uint64_t)coord.x - (uint64_t)camera_origin.x;
    uint64_t ry = (uint64_t)coord.y - (uint64_t)camera_origin.y;
    uint64_t dx = coord.x < camera_origin.x ?
        (uint64_t)camera_origin.x - (uint64_t)coord.x :
        rx >= VIEW_CHUNKS_X ? rx - VIEW_CHUNKS_X + 1u : 0u;
    uint64_t dy = coord.y < camera_origin.y ?
        (uint64_t)camera_origin.y - (uint64_t)coord.y :
        ry >= VIEW_CHUNKS_Y ? ry - VIEW_CHUNKS_Y + 1u : 0u;
    if (dx <= VIEW_CHUNKS_X && dy <= VIEW_CHUNKS_Y) return 1u;
    if (dx <= 2u * VIEW_CHUNKS_X && dy <= 2u * VIEW_CHUNKS_Y) return 2u;
    if (dx <= 4u * VIEW_CHUNKS_X && dy <= 4u * VIEW_CHUNKS_Y) return 3u;
    return 5u;
}

uint32_t dc_offscreen_band(const dc_offscreen_t *offscreen,
                           dc_chunk_coord_t camera_origin) {
    if (!offscreen || !offscreen->count) return 5u;
    uint32_t closest = 5u;
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i) {
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
    return dc_offscreen_update_batch(&offscreen, 1u, camera_origin,
                                     elapsed_seconds, catch_up, err, cap);
}

bool dc_offscreen_update_batch(dc_offscreen_t *const *workspaces, uint32_t count,
                               dc_chunk_coord_t camera_origin,
                               double elapsed_seconds, bool catch_up,
                               char *err, uint32_t cap) {
    if (!workspaces || !count || count > DC_OFFSCREEN_MAX_CLUSTERS ||
        !(elapsed_seconds >= 0.0))
        return error(err, cap, "Invalid offscreen update batch");
    uint32_t bands[DC_OFFSCREEN_MAX_CLUSTERS] = {0};
    for (uint32_t i = 0u; i < count; ++i) {
        dc_offscreen_t *offscreen = workspaces[i];
        if (!offscreen) continue;
        offscreen->last_advance_ticks = 0.0f;
        bands[i] = dc_offscreen_band(offscreen, camera_origin);
        if (bands[i] == 5u) { offscreen->pending_seconds = 0.0; continue; }
        offscreen->pending_seconds += elapsed_seconds;
        uint32_t sweeps = bands[i] == 1u ? 8u : bands[i] == 2u ? 6u : 4u;
        if (!dc_gpu_set_pressure_sweeps(offscreen->gpu, sweeps))
            return error(err, cap, "Cannot set offscreen pressure work");
    }
    for (;;) {
        dc_gpu_tick_batch_item_t items[DC_OFFSCREEN_MAX_CLUSTERS];
        dc_offscreen_t *planned[DC_OFFSCREEN_MAX_CLUSTERS];
        double advances[DC_OFFSCREEN_MAX_CLUSTERS];
        uint32_t used = 0u;
        for (uint32_t i = 0u; i < count; ++i) {
            dc_offscreen_t *offscreen = workspaces[i];
            if (!offscreen || bands[i] == 5u) continue;
            uint32_t cadence = bands[i] == 1u ? 4u : bands[i] == 2u ? 12u : 24u;
            double period = (double)cadence / 60.0;
            uint32_t substeps = (uint32_t)(period * 20.0 + 0.999999);
            if (!substeps) substeps = 1u;
            uint32_t steps = 0u;
            double advance = 0.0;
            if (offscreen->pending_seconds + 1e-7 >= period) {
                uint32_t due = (uint32_t)((offscreen->pending_seconds + 1e-7) /
                                           period);
                uint32_t batch = OFFSCREEN_MAX_BATCH_STEPS / substeps;
                if (due < batch) batch = due;
                steps = batch * substeps;
                advance = period * batch;
            } else if (catch_up && offscreen->pending_seconds > 1e-7) {
                advance = offscreen->pending_seconds;
                steps = (uint32_t)(advance * 20.0 + 0.999999);
                if (!steps) steps = 1u;
            }
            if (!steps) continue;
            if (!dc_gpu_set_tick_seconds(offscreen->gpu,
                                         (float)(advance / steps)))
                return error(err, cap, "Invalid offscreen tick duration");
            items[used] = (dc_gpu_tick_batch_item_t){offscreen->gpu, steps};
            planned[used] = offscreen;
            advances[used] = advance;
            ++used;
        }
        if (!used) return true;
        if (!dc_gpu_tick_batch(items, used, err, cap)) return false;
        for (uint32_t i = 0u; i < used; ++i) {
            planned[i]->pending_seconds -= advances[i];
            if (planned[i]->pending_seconds < 1e-7)
                planned[i]->pending_seconds = 0.0;
            planned[i]->last_advance_ticks += (float)(advances[i] * 60.0);
        }
    }
}

bool dc_offscreen_flush(dc_offscreen_t *offscreen, dc_offscreen_save_fn save,
                        void *context, char *err, uint32_t cap) {
    if (!offscreen || !save) return error(err, cap, "Invalid offscreen flush");
    dc_chunk_t *chunk = malloc(sizeof(*chunk));
    if (!chunk) return error(err, cap, "Cannot allocate offscreen save chunk");
    for (uint32_t i = 0; i < OFFSCREEN_SLOTS; ++i) {
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
