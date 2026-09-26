#ifndef DUNGEONCRAFT_APP_LEVEL_H
#define DUNGEONCRAFT_APP_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"
#include "dungeoncraft/gpu.h"

typedef struct dc_level_view dc_level_view_t;

typedef struct {
    dc_chunk_coord_t origin;
    uint32_t offset_x, offset_y;
    uint32_t ready_chunks, total_chunks;
} dc_level_view_status_t;

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t err_cap);
bool dc_level_view_tick(dc_level_view_t *view, char *err, uint32_t err_cap);
bool dc_level_view_step(dc_level_view_t *view, char *err, uint32_t err_cap);
bool dc_level_view_spawn_body(dc_level_view_t *view, uint32_t x, uint32_t y,
                              char *err, uint32_t err_cap);
bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t err_cap);
bool dc_level_view_move(dc_level_view_t *view, int32_t dx, int32_t dy);
bool dc_level_view_pan_pixels(dc_level_view_t *view, int32_t dx, int32_t dy);
bool dc_level_view_reset_camera(dc_level_view_t *view);
bool dc_level_view_status(dc_level_view_t *view, dc_level_view_status_t *status);
bool dc_level_view_set_title(dc_level_view_t *view, const char *title);
bool dc_level_view_toggle_marker_overlay(dc_level_view_t *view);
bool dc_level_view_set_zoom(dc_level_view_t *view, uint32_t zoom);
bool dc_level_view_screen_cell(dc_level_view_t *view, uint32_t screen_x,
                               uint32_t screen_y, uint32_t *cell_x,
                               uint32_t *cell_y);
bool dc_level_view_set_overlay(dc_level_view_t *view, dc_gpu_overlay_t overlay);
bool dc_level_view_set_spring_enabled(dc_level_view_t *view, bool enabled);
bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material,
                         char *err, uint32_t err_cap);
bool dc_level_view_pixel(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t *color, char *err, uint32_t err_cap);
bool dc_level_view_pixels(dc_level_view_t *view, uint32_t *colors,
                          uint32_t count, char *err, uint32_t err_cap);
bool dc_level_view_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                         dc_chunk_t *chunk, char *err, uint32_t err_cap);
bool dc_level_view_has_chunk(dc_level_view_t *view, dc_chunk_coord_t coord);
/** Queue an adjacent world-cell transfer across streamed chunks. */
bool dc_level_view_queue_transfer(dc_level_view_t *view, int64_t from_x, int64_t from_y,
                                  int64_t to_x, int64_t to_y, uint32_t amount,
                                  dc_gpu_transfer_kind_t kind, char *err, uint32_t cap);
/** Return true once the queued transfer was applied or rejected by cell contents. */
bool dc_level_view_transfer_result(dc_level_view_t *view,
                                   dc_gpu_transfer_state_t *state);
bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t err_cap);

#endif
