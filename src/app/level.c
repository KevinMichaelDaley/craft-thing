#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"
#include "level.h"
#include "view_config.h"

_Static_assert(DC_GPU_CHUNK_SLOTS >= SIM_CHUNKS_X * SIM_CHUNKS_Y,
               "GPU chunk pool must cover viewport and halo");

struct dc_level_view {
    dc_gpu_t *gpu;
    dc_streamer_t *stream;
    dc_chunk_table_t table;
    dc_chunk_coord_t origin;
    dc_chunk_coord_t mapped_origin;
    uint32_t camera_offset_x, camera_offset_y;
    int32_t pending_chunk_dx, pending_chunk_dy;
    uint32_t pending_steps;
    float pending_seconds;
    bool marker_overlay;
    bool spring_enabled;
    bool transfer_pending, transfer_complete;
    dc_gpu_transfer_state_t transfer_state;
    dc_chunk_coord_t transfer_from, transfer_to;
    uint32_t from_x, from_y, to_x, to_y, transfer_amount, transfer_kind;
    uint32_t from_slot, to_slot;
    uint64_t from_generation, to_generation;
    bool destination_bound;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_level_view_step(dc_level_view_t *view, char *err, uint32_t cap) {
    return dc_level_view_step_timed(view, 1.0f / 60.0f, err, cap);
}

bool dc_level_view_step_timed(dc_level_view_t *view, float seconds,
                              char *err, uint32_t cap) {
    if (!view) return error(err, cap, "Level view is null");
    if (view->pending_steps == UINT32_MAX)
        return error(err, cap, "Too many pending level steps");
    ++view->pending_steps;
    view->pending_seconds += seconds;
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_mark_dirty(&view->table, i);
    return true;
}

bool dc_level_view_spawn_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                              char *err, uint32_t cap) {
    if (!view || x > VIEW_WIDTH - 4 || y > VIEW_HEIGHT - 4)
        return error(err, cap, "Invalid rigid body spawn position");
    dc_gpu_body_t body = { .x_fp = (int32_t)(x + DC_CHUNK_SIDE +
                                           view->camera_offset_x) << 16,
        .y_fp = (int32_t)(y + DC_CHUNK_SIDE +
                          view->camera_offset_y) << 16, .vx_fp = 1 << 16,
        .width = 4, .height = 4, .id = 1, .active = 1 };
    return dc_gpu_spawn_body(view->gpu, body, err, cap);
}

static bool visible(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return coord.x >= view->origin.x - HALO_CHUNKS &&
           coord.x < view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS &&
           coord.y >= view->origin.y - HALO_CHUNKS &&
           coord.y < view->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS;
}

static bool process_results(dc_level_view_t *view, char *err, uint32_t cap) {
    dc_stream_result_t result;
    while (dc_stream_poll(view->stream, &result)) {
        uint32_t index;
        if (!dc_chunk_table_find(&view->table, result.coord, &index) ||
            view->table.slots[index].generation != result.generation) {
            dc_stream_result_release(&result);
            continue;
        }
        if (result.kind == DC_STREAM_FAILED) {
            dc_stream_result_release(&result);
            return error(err, cap, "Chunk worker load or save failed");
        }
        if (result.kind == DC_STREAM_LOADED) {
            if (!dc_gpu_upload_chunk(view->gpu, index, result.chunk, err, cap) ||
                !dc_chunk_table_finish_load(&view->table, index, result.generation)) {
                dc_stream_result_release(&result);
                return error(err, cap, "Cannot publish loaded GPU chunk");
            }
        } else if (!dc_chunk_table_finish_save(&view->table, index, result.generation)) {
            dc_stream_result_release(&result);
            return error(err, cap, "Cannot complete chunk save");
        }
        dc_stream_result_release(&result);
    }
    return true;
}

static bool schedule_saves(dc_level_view_t *view, char *err, uint32_t cap) {
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state != DC_SLOT_SLEEPING || !slot->dirty || slot->pinned) continue;
        dc_chunk_t chunk = { .coord = slot->coord };
        uint64_t generation;
        if (!dc_gpu_download_chunk(view->gpu, i, &chunk, err, cap)) return false;
        if (!dc_chunk_table_begin_save(&view->table, i, &generation))
            return error(err, cap, "Cannot begin dirty chunk save");
        if (!dc_stream_request_save(view->stream, &chunk, generation))
            return error(err, cap, "Cannot queue dirty chunk save");
    }
    return true;
}

static bool transfer_slot_matches(const dc_level_view_t *view, uint32_t index,
                                  uint64_t generation, dc_chunk_coord_t coord) {
    const dc_chunk_slot_t *slot = &view->table.slots[index];
    return slot->state != DC_SLOT_EMPTY && slot->generation == generation &&
           slot->coord.x == coord.x && slot->coord.y == coord.y;
}

static bool bind_transfer_destination(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view->transfer_pending || view->destination_bound) return true;
    uint32_t index;
    if (!dc_chunk_table_find(&view->table, view->transfer_to, &index)) {
        uint64_t generation;
        if (!dc_chunk_table_begin_load(&view->table, view->transfer_to,
                                       &index, &generation)) return true;
        if (!dc_stream_request_load(view->stream, view->transfer_to, generation))
            return error(err, cap, "Cannot load transfer destination");
    }
    view->to_slot = index;
    view->to_generation = view->table.slots[index].generation;
    view->table.slots[index].pinned = true;
    view->destination_bound = true;
    return true;
}

static bool resolve_world_transfer(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view->transfer_pending || !view->destination_bound) return true;
    if (!transfer_slot_matches(view, view->from_slot, view->from_generation,
                               view->transfer_from) ||
        !transfer_slot_matches(view, view->to_slot, view->to_generation,
                               view->transfer_to))
        return error(err, cap, "Pinned transfer chunk was reused");
    dc_chunk_slot_t *source = &view->table.slots[view->from_slot];
    dc_chunk_slot_t *destination = &view->table.slots[view->to_slot];
    if ((source->state != DC_SLOT_ACTIVE && source->state != DC_SLOT_SLEEPING) ||
        (destination->state != DC_SLOT_ACTIVE &&
         destination->state != DC_SLOT_SLEEPING)) return true;
    dc_gpu_transfer_t command = { .from_x = view->from_x, .from_y = view->from_y,
        .to_x = view->to_x, .to_y = view->to_y, .amount = view->transfer_amount,
        .kind = view->transfer_kind, .from_slot = view->from_slot,
        .to_slot = view->to_slot };
    dc_gpu_transfer_state_t state;
    if (!dc_gpu_queue_slot_transfer(view->gpu, command, err, cap) ||
        !dc_gpu_try_transfer(view->gpu, &state, err, cap)) return false;
    if (state == DC_GPU_TRANSFER_PENDING) return true;
    if (state == DC_GPU_TRANSFER_APPLIED &&
        (!dc_chunk_table_mark_dirty(&view->table, view->from_slot) ||
         !dc_chunk_table_mark_dirty(&view->table, view->to_slot)))
        return error(err, cap, "Cannot mark transferred chunks dirty");
    source->pinned = false;
    destination->pinned = false;
    view->transfer_pending = false;
    view->transfer_complete = true;
    view->transfer_state = state;
    return true;
}

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t cap) {
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) { error(err, cap, "Out of memory creating level view"); return NULL; }
    view->origin.y = INITIAL_CHUNK_Y;
    view->mapped_origin.y = INITIAL_CHUNK_Y;
    if (!dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS) ||
        !dc_gpu_create_window(&view->gpu, SIM_WIDTH, SIM_HEIGHT,
            VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
            "build/shaders/pattern.comp.spv", err, cap) ||
        !dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
                             VIEW_WIDTH, VIEW_HEIGHT) ||
        !dc_gpu_set_display_zoom(view->gpu, WINDOW_SCALE) ||
        !dc_gpu_set_fluid_interval(view->gpu, FLUID_INTERVAL)) goto fail;
    view->spring_enabled = true;
    view->stream = dc_stream_create(directory, seed,
                                    DC_GPU_CHUNK_SLOTS * 2u);
    if (!view->stream) { error(err, cap, "Cannot start chunk streaming worker"); goto fail; }
    return view;
fail:
    dc_stream_destroy(view->stream);
    dc_gpu_destroy(view->gpu);
    dc_chunk_table_destroy(&view->table);
    free(view);
    return NULL;
}

bool dc_level_view_tick(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return error(err, cap, "Level view is null");
    if (!process_results(view, err, cap)) return false;
    if (!bind_transfer_destination(view, err, cap)) return false;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state == DC_SLOT_ACTIVE || slot->state == DC_SLOT_SLEEPING)
            dc_chunk_table_set_active(&view->table, i, visible(view, slot->coord));
    }
    if (!schedule_saves(view, err, cap)) return false;
    for (uint32_t i = 0; i < view->table.capacity; ++i) {
        dc_chunk_slot_t *slot = &view->table.slots[i];
        if (slot->state == DC_SLOT_SLEEPING && !slot->dirty && !slot->pinned &&
            !dc_chunk_table_evict(&view->table, i, UINT64_MAX))
            return error(err, cap, "Cannot evict clean sleeping chunk");
    }
    if ((view->pending_chunk_dx || view->pending_chunk_dy) &&
        !dc_gpu_shift_velocity(view->gpu, view->pending_chunk_dx,
                                view->pending_chunk_dy, err, cap)) return false;
    bool all_resident = true;
    for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y) {
        for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = {
                view->origin.x + (int64_t)x - HALO_CHUNKS,
                view->origin.y + (int64_t)y - HALO_CHUNKS };
            uint32_t index;
            if (!dc_chunk_table_find(&view->table, coord, &index)) {
                uint64_t generation;
                if (dc_chunk_table_begin_load(&view->table, coord, &index, &generation) &&
                    !dc_stream_request_load(view->stream, coord, generation))
                    return error(err, cap, "Cannot queue chunk load");
            }
            uint32_t page = UINT32_MAX;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE) page = index;
            if (page == UINT32_MAX) all_resident = false;
            if (!dc_gpu_set_page(view->gpu, x, y, page, err, cap)) return false;
        }
    }
    view->pending_chunk_dx = 0;
    view->pending_chunk_dy = 0;
    view->mapped_origin = view->origin;
    if (!resolve_world_transfer(view, err, cap)) return false;
    if (view->spring_enabled && view->origin.x - HALO_CHUNKS <= 2 * WORLD_SCALE &&
        view->origin.x + VIEW_CHUNKS_X + HALO_CHUNKS > 2 * WORLD_SCALE &&
        view->origin.y - HALO_CHUNKS <= 0 &&
        view->origin.y + VIEW_CHUNKS_Y + HALO_CHUNKS > 0) {
        uint32_t x = (uint32_t)(2 * WORLD_SCALE - view->origin.x + HALO_CHUNKS) * DC_CHUNK_SIDE;
        uint32_t y = (uint32_t)(-view->origin.y + HALO_CHUNKS) * DC_CHUNK_SIDE +
                     4u * WORLD_SCALE;
        dc_gpu_set_tick_water_source(view->gpu, true, x, y);
    } else dc_gpu_set_tick_water_source(view->gpu, false, 0, 0);
    uint32_t ready_steps = all_resident ? view->pending_steps : 0u;
    if (ready_steps && !dc_gpu_set_tick_seconds(view->gpu,
            view->pending_seconds / (float)ready_steps))
        return error(err, cap, "Invalid elapsed simulation time");
    if (!dc_gpu_present_chunks_steps(view->gpu, ready_steps, err, cap)) return false;
    view->pending_steps = 0;
    view->pending_seconds = 0.0f;
    return true;
}

bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t cap) {
    uint64_t deadline = SDL_GetTicks64() + timeout_ms;
    do {
        if (!dc_level_view_tick(view, err, cap)) return false;
        bool ready = true;
        for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y) {
            for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
                dc_chunk_coord_t coord = {
                    view->origin.x + (int64_t)x - HALO_CHUNKS,
                    view->origin.y + (int64_t)y - HALO_CHUNKS };
                uint32_t index;
                if (!dc_chunk_table_find(&view->table, coord, &index) ||
                    view->table.slots[index].state != DC_SLOT_ACTIVE) ready = false;
            }
        }
        if (ready) return true;
        SDL_Delay(1);
    } while (SDL_GetTicks64() < deadline);
    return error(err, cap, "Timed out waiting for visible chunks");
}

bool dc_level_view_move(dc_level_view_t *view, int32_t dx, int32_t dy) {
    if (!view || dx < -1 || dx > 1 || dy < -1 || dy > 1) return false;
    if ((dx < 0 && view->origin.x <= INT64_MIN + HALO_CHUNKS) ||
        (dy < 0 && view->origin.y <= INT64_MIN + HALO_CHUNKS) ||
        (dx > 0 && view->origin.x >= INT64_MAX - VIEW_CHUNKS_X - HALO_CHUNKS) ||
        (dy > 0 && view->origin.y >= INT64_MAX - VIEW_CHUNKS_Y - HALO_CHUNKS)) return false;
    view->origin.x += dx;
    view->origin.y += dy;
    view->pending_chunk_dx += dx;
    view->pending_chunk_dy += dy;
    return true;
}

bool dc_level_view_pan_pixels(dc_level_view_t *view, int32_t dx, int32_t dy) {
    if (!view || dx < -1024 || dx > 1024 || dy < -1024 || dy > 1024)
        return false;
    dc_chunk_coord_t shift;
    uint32_t offset_x, offset_y;
    dc_cell_address((int64_t)view->camera_offset_x + dx,
                    (int64_t)view->camera_offset_y + dy,
                    &shift, &offset_x, &offset_y);
    const int64_t min_origin = INT64_MIN + HALO_CHUNKS;
    const int64_t max_x = INT64_MAX - VIEW_CHUNKS_X - HALO_CHUNKS;
    const int64_t max_y = INT64_MAX - VIEW_CHUNKS_Y - HALO_CHUNKS;
    if ((shift.x < 0 && view->origin.x < min_origin - shift.x) ||
        (shift.x > 0 && view->origin.x > max_x - shift.x) ||
        (shift.y < 0 && view->origin.y < min_origin - shift.y) ||
        (shift.y > 0 && view->origin.y > max_y - shift.y) ||
        (shift.x > 0 && view->pending_chunk_dx > INT32_MAX - shift.x) ||
        (shift.x < 0 && view->pending_chunk_dx < INT32_MIN - shift.x) ||
        (shift.y > 0 && view->pending_chunk_dy > INT32_MAX - shift.y) ||
        (shift.y < 0 && view->pending_chunk_dy < INT32_MIN - shift.y))
        return false;
    if (!dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE + offset_x,
                             DC_CHUNK_SIDE + offset_y,
                             VIEW_WIDTH, VIEW_HEIGHT)) return false;
    view->origin.x += shift.x;
    view->origin.y += shift.y;
    view->camera_offset_x = offset_x;
    view->camera_offset_y = offset_y;
    view->pending_chunk_dx += (int32_t)shift.x;
    view->pending_chunk_dy += (int32_t)shift.y;
    return true;
}

bool dc_level_view_reset_camera(dc_level_view_t *view) {
    if (!view || !dc_gpu_set_viewport(view->gpu, DC_CHUNK_SIDE,
                                       DC_CHUNK_SIDE, VIEW_WIDTH,
                                       VIEW_HEIGHT)) return false;
    view->pending_chunk_dx = view->mapped_origin.x > SIM_CHUNKS_X ?
        -SIM_CHUNKS_X : view->mapped_origin.x < -SIM_CHUNKS_X ?
        SIM_CHUNKS_X : (int32_t)-view->mapped_origin.x;
    int64_t mapped_offset_y = view->mapped_origin.y - INITIAL_CHUNK_Y;
    view->pending_chunk_dy = mapped_offset_y > SIM_CHUNKS_Y ?
        -SIM_CHUNKS_Y : mapped_offset_y < -SIM_CHUNKS_Y ?
        SIM_CHUNKS_Y : (int32_t)-mapped_offset_y;
    view->origin = (dc_chunk_coord_t){0, INITIAL_CHUNK_Y};
    view->camera_offset_x = 0;
    view->camera_offset_y = 0;
    return true;
}

bool dc_level_view_status(dc_level_view_t *view, dc_level_view_status_t *status) {
    if (!view || !status) return false;
    *status = (dc_level_view_status_t){ .origin = view->origin,
        .offset_x = view->camera_offset_x,
        .offset_y = view->camera_offset_y,
        .total_chunks = SIM_CHUNKS_X * SIM_CHUNKS_Y };
    for (uint32_t y = 0; y < SIM_CHUNKS_Y; ++y)
        for (uint32_t x = 0; x < SIM_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = {
                view->origin.x + (int64_t)x - HALO_CHUNKS,
                view->origin.y + (int64_t)y - HALO_CHUNKS };
            uint32_t index;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE)
                ++status->ready_chunks;
        }
    return true;
}

bool dc_level_view_set_title(dc_level_view_t *view, const char *title) {
    return view && dc_gpu_set_window_title(view->gpu, title);
}

bool dc_level_view_toggle_marker_overlay(dc_level_view_t *view) {
    if (!view) return false;
    view->marker_overlay = !view->marker_overlay;
    return dc_gpu_set_marker_overlay(view->gpu, view->marker_overlay);
}

bool dc_level_view_set_zoom(dc_level_view_t *view, uint32_t zoom) {
    return view && dc_gpu_set_display_zoom(view->gpu, zoom);
}

bool dc_level_view_screen_cell(dc_level_view_t *view, uint32_t screen_x,
                               uint32_t screen_y, uint32_t *cell_x,
                               uint32_t *cell_y) {
    return view && dc_gpu_screen_cell(view->gpu, screen_x, screen_y,
                                      cell_x, cell_y);
}

bool dc_level_view_set_overlay(dc_level_view_t *view, dc_gpu_overlay_t overlay) {
    return view && dc_gpu_set_overlay(view->gpu, overlay);
}

bool dc_level_view_set_spring_enabled(dc_level_view_t *view, bool enabled) {
    if (!view) return false;
    view->spring_enabled = enabled;
    return true;
}

bool dc_level_view_set_marker_correction(dc_level_view_t *view, bool enabled) {
    return view && dc_gpu_set_marker_correction(view->gpu, enabled);
}

static int32_t cell_chunk_offset(int32_t coordinate) {
    return coordinate >= 0 ? coordinate / DC_CHUNK_SIDE :
           -((-coordinate + DC_CHUNK_SIDE - 1) / DC_CHUNK_SIDE);
}

bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material, char *err, uint32_t cap) {
    if (!view || x >= VIEW_WIDTH || y >= VIEW_HEIGHT || radius > 16)
        return error(err, cap, "Invalid material brush coordinates");
    if (!dc_gpu_paint_material(view->gpu, x + DC_CHUNK_SIDE + view->camera_offset_x,
                               y + DC_CHUNK_SIDE + view->camera_offset_y,
                               radius, material, err, cap)) return false;
    int32_t first_x = cell_chunk_offset((int32_t)x +
        (int32_t)view->camera_offset_x - (int32_t)radius);
    int32_t first_y = cell_chunk_offset((int32_t)y +
        (int32_t)view->camera_offset_y - (int32_t)radius);
    int32_t last_x = cell_chunk_offset((int32_t)x +
        (int32_t)view->camera_offset_x + (int32_t)radius);
    int32_t last_y = cell_chunk_offset((int32_t)y +
        (int32_t)view->camera_offset_y + (int32_t)radius);
    for (int32_t ty = first_y; ty <= last_y; ++ty) {
        for (int32_t tx = first_x; tx <= last_x; ++tx) {
            dc_chunk_coord_t coord = { view->origin.x + tx, view->origin.y + ty };
            uint32_t index;
            if (dc_chunk_table_find(&view->table, coord, &index) &&
                view->table.slots[index].state == DC_SLOT_ACTIVE)
                dc_chunk_table_mark_dirty(&view->table, index);
        }
    }
    return true;
}

bool dc_level_view_pixel(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t *color, char *err, uint32_t cap) {
    if (!view || !color || x >= VIEW_WIDTH || y >= VIEW_HEIGHT)
        return error(err, cap, "Invalid pixel readback");
    uint32_t *pixels = malloc((size_t)VIEW_WIDTH * VIEW_HEIGHT * sizeof(*pixels));
    if (!pixels) return error(err, cap, "Out of memory reading level pixel");
    bool okay = dc_gpu_readback(view->gpu, pixels,
                                VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    if (okay) *color = pixels[y * VIEW_WIDTH + x];
    free(pixels);
    return okay;
}

bool dc_level_view_pixels(dc_level_view_t *view, uint32_t *colors,
                          uint32_t count, char *err, uint32_t cap) {
    if (!view || !colors || count < VIEW_WIDTH * VIEW_HEIGHT)
        return error(err, cap, "Invalid level image readback");
    return dc_gpu_readback(view->gpu, colors, count, err, cap);
}

bool dc_level_view_capture_tick(dc_level_view_t *view,
                                dc_gpu_tick_capture_t *capture,
                                char *err, uint32_t cap) {
    return view && dc_gpu_tick_capture(view->gpu, capture, err, cap);
}

bool dc_level_view_memory_stats(dc_level_view_t *view,
                                dc_gpu_memory_stats_t *stats) {
    return view && dc_gpu_memory_stats(view->gpu, stats);
}

bool dc_level_view_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                         dc_chunk_t *chunk, char *err, uint32_t cap) {
    uint32_t index;
    if (!view || !chunk || !dc_chunk_table_find(&view->table, coord, &index) ||
        (view->table.slots[index].state != DC_SLOT_ACTIVE &&
         view->table.slots[index].state != DC_SLOT_SLEEPING))
        return error(err, cap, "Requested chunk is not resident");
    chunk->coord = coord;
    return dc_gpu_download_chunk(view->gpu, index, chunk, err, cap);
}

bool dc_level_view_has_chunk(dc_level_view_t *view, dc_chunk_coord_t coord) {
    return view && dc_chunk_table_find(&view->table, coord, NULL);
}

bool dc_level_view_queue_transfer(dc_level_view_t *view, int64_t from_x, int64_t from_y,
                                  int64_t to_x, int64_t to_y, uint32_t amount,
                                  dc_gpu_transfer_kind_t kind, char *err, uint32_t cap) {
    if (!view || view->transfer_pending ||
        (kind != DC_GPU_TRANSFER_SCALAR && kind != DC_GPU_TRANSFER_PARTICLE) ||
        (kind == DC_GPU_TRANSFER_SCALAR && (!amount || amount > DC_FLUID_FULL)))
        return error(err, cap, "Invalid world transfer");
    bool adjacent =
        (from_y == to_y &&
         ((from_x < INT64_MAX && to_x == from_x + 1) ||
          (from_x > INT64_MIN && to_x == from_x - 1))) ||
        (from_x == to_x &&
         ((from_y < INT64_MAX && to_y == from_y + 1) ||
          (from_y > INT64_MIN && to_y == from_y - 1)));
    if (!adjacent) return error(err, cap, "World transfer cells must be adjacent");
    dc_chunk_coord_t from, to;
    uint32_t from_local_x, from_local_y, to_local_x, to_local_y;
    dc_cell_address(from_x, from_y, &from, &from_local_x, &from_local_y);
    dc_cell_address(to_x, to_y, &to, &to_local_x, &to_local_y);
    uint32_t index;
    if (!dc_chunk_table_find(&view->table, from, &index) ||
        (view->table.slots[index].state != DC_SLOT_ACTIVE &&
         view->table.slots[index].state != DC_SLOT_SLEEPING) ||
        view->table.slots[index].pinned)
        return error(err, cap, "Transfer source is not available");
    view->from_slot = index;
    view->from_generation = view->table.slots[index].generation;
    view->table.slots[index].pinned = true;
    view->transfer_from = from;
    view->transfer_to = to;
    view->from_x = from_local_x;
    view->from_y = from_local_y;
    view->to_x = to_local_x;
    view->to_y = to_local_y;
    view->transfer_amount = amount;
    view->transfer_kind = kind;
    view->destination_bound = false;
    view->transfer_complete = false;
    view->transfer_pending = true;
    return bind_transfer_destination(view, err, cap);
}

bool dc_level_view_transfer_result(dc_level_view_t *view,
                                   dc_gpu_transfer_state_t *state) {
    if (!view || !state || !view->transfer_complete) return false;
    *state = view->transfer_state;
    return true;
}

bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return true;
    bool okay = true;
    if (view->transfer_pending) {
        view->table.slots[view->from_slot].pinned = false;
        if (view->destination_bound)
            view->table.slots[view->to_slot].pinned = false;
        view->transfer_pending = false;
    }
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_set_active(&view->table, i, false);
    uint64_t deadline = SDL_GetTicks64() +
                        (uint64_t)view->table.capacity * 50u + 5000u;
    for (;;) {
        if (!process_results(view, err, cap) || !schedule_saves(view, err, cap)) { okay = false; break; }
        bool pending = false;
        for (uint32_t i = 0; i < view->table.capacity; ++i) {
            dc_chunk_slot_t *slot = &view->table.slots[i];
            if (slot->dirty || slot->state == DC_SLOT_SAVING) pending = true;
        }
        if (!pending) break;
        if (SDL_GetTicks64() >= deadline) {
            okay = error(err, cap, "Timed out saving modified chunks"); break;
        }
        SDL_Delay(1);
    }
    dc_stream_destroy(view->stream);
    dc_gpu_destroy(view->gpu);
    dc_chunk_table_destroy(&view->table);
    free(view);
    return okay;
}
