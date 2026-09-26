#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/stream.h"
#include "level.h"

enum { VIEW_WIDTH = 256, VIEW_HEIGHT = 128, WINDOW_SCALE = 4,
       VIEW_CHUNKS_X = VIEW_WIDTH / DC_CHUNK_SIDE,
       VIEW_CHUNKS_Y = VIEW_HEIGHT / DC_CHUNK_SIDE };

struct dc_level_view {
    dc_gpu_t *gpu;
    dc_streamer_t *stream;
    dc_chunk_table_t table;
    dc_chunk_coord_t origin;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_level_view_step(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return error(err, cap, "Level view is null");
    if (view->origin.x <= 2 && view->origin.x + VIEW_CHUNKS_X > 2 &&
        view->origin.y <= 0 && view->origin.y + VIEW_CHUNKS_Y > 0) {
        uint32_t x = (uint32_t)(2 - view->origin.x) * DC_CHUNK_SIDE;
        uint32_t y = (uint32_t)(-view->origin.y) * DC_CHUNK_SIDE + 4u;
        if (!dc_gpu_paint_material(view->gpu, x, y, 1, DC_MATERIAL_WATER,
                                   err, cap)) return false;
    }
    dc_gpu_tick_capture_t capture = {0};
    if (!dc_gpu_tick_capture(view->gpu, &capture, err, cap)) return false;
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_mark_dirty(&view->table, i);
    return true;
}

bool dc_level_view_spawn_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                              char *err, uint32_t cap) {
    if (!view || x > VIEW_WIDTH - 4 || y > VIEW_HEIGHT - 4)
        return error(err, cap, "Invalid rigid body spawn position");
    dc_gpu_body_t body = { .x_fp = (int32_t)x << 16,
        .y_fp = (int32_t)y << 16, .vx_fp = 1 << 16,
        .width = 4, .height = 4, .id = 1, .active = 1 };
    return dc_gpu_spawn_body(view->gpu, body, err, cap);
}

static bool visible(const dc_level_view_t *view, dc_chunk_coord_t coord) {
    return coord.x >= view->origin.x &&
           coord.x <= view->origin.x + VIEW_CHUNKS_X - 1 &&
           coord.y >= view->origin.y &&
           coord.y <= view->origin.y + VIEW_CHUNKS_Y - 1;
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
        if (slot->state != DC_SLOT_SLEEPING || !slot->dirty) continue;
        dc_chunk_t chunk = { .coord = slot->coord };
        uint64_t generation;
        if (!dc_gpu_download_chunk(view->gpu, i, &chunk, err, cap) ||
            !dc_chunk_table_begin_save(&view->table, i, &generation) ||
            !dc_stream_request_save(view->stream, &chunk, generation))
            return error(err, cap, "Cannot queue dirty chunk save");
    }
    return true;
}

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t cap) {
    dc_level_view_t *view = calloc(1, sizeof(*view));
    if (!view) { error(err, cap, "Out of memory creating level view"); return NULL; }
    if (!dc_chunk_table_init(&view->table, DC_GPU_CHUNK_SLOTS) ||
        !dc_gpu_create_window(&view->gpu, VIEW_WIDTH, VIEW_HEIGHT,
            VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
            "build/shaders/pattern.comp.spv", err, cap)) goto fail;
    view->stream = dc_stream_create(directory, seed, 128);
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
    for (uint32_t y = 0; y < VIEW_CHUNKS_Y; ++y) {
        for (uint32_t x = 0; x < VIEW_CHUNKS_X; ++x) {
            dc_chunk_coord_t coord = { view->origin.x + x, view->origin.y + y };
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
            if (!dc_gpu_set_page(view->gpu, x, y, page, err, cap)) return false;
        }
    }
    return dc_gpu_render_chunks(view->gpu, err, cap) && dc_gpu_present(view->gpu, err, cap);
}

bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t cap) {
    uint64_t deadline = SDL_GetTicks64() + timeout_ms;
    do {
        if (!dc_level_view_tick(view, err, cap)) return false;
        bool ready = true;
        for (uint32_t y = 0; y < VIEW_CHUNKS_Y; ++y) {
            for (uint32_t x = 0; x < VIEW_CHUNKS_X; ++x) {
                dc_chunk_coord_t coord = { view->origin.x + x, view->origin.y + y };
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
    if ((dx < 0 && view->origin.x == INT64_MIN) ||
        (dy < 0 && view->origin.y == INT64_MIN) ||
        (dx > 0 && view->origin.x >= INT64_MAX - VIEW_CHUNKS_X) ||
        (dy > 0 && view->origin.y >= INT64_MAX - VIEW_CHUNKS_Y)) return false;
    view->origin.x += dx;
    view->origin.y += dy;
    return true;
}

bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material, char *err, uint32_t cap) {
    if (!view || x >= VIEW_WIDTH || y >= VIEW_HEIGHT || radius > 16)
        return error(err, cap, "Invalid material brush coordinates");
    if (!dc_gpu_paint_material(view->gpu, x, y, radius, material, err, cap)) return false;
    uint32_t min_x = x > radius ? x - radius : 0;
    uint32_t min_y = y > radius ? y - radius : 0;
    uint32_t max_x = x + radius < VIEW_WIDTH ? x + radius : VIEW_WIDTH - 1;
    uint32_t max_y = y + radius < VIEW_HEIGHT ? y + radius : VIEW_HEIGHT - 1;
    for (uint32_t ty = min_y / DC_CHUNK_SIDE; ty <= max_y / DC_CHUNK_SIDE; ++ty) {
        for (uint32_t tx = min_x / DC_CHUNK_SIDE; tx <= max_x / DC_CHUNK_SIDE; ++tx) {
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
    uint32_t pixels[VIEW_WIDTH * VIEW_HEIGHT];
    if (!dc_gpu_readback(view->gpu, pixels, VIEW_WIDTH * VIEW_HEIGHT, err, cap)) return false;
    *color = pixels[y * VIEW_WIDTH + x];
    return true;
}

bool dc_level_view_pixels(dc_level_view_t *view, uint32_t *colors,
                          uint32_t count, char *err, uint32_t cap) {
    if (!view || !colors || count < VIEW_WIDTH * VIEW_HEIGHT)
        return error(err, cap, "Invalid level image readback");
    return dc_gpu_readback(view->gpu, colors, count, err, cap);
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

bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!view) return true;
    bool okay = true;
    for (uint32_t i = 0; i < view->table.capacity; ++i)
        if (view->table.slots[i].state == DC_SLOT_ACTIVE)
            dc_chunk_table_set_active(&view->table, i, false);
    uint64_t deadline = SDL_GetTicks64() + 5000;
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
