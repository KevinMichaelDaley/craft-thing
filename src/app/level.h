#ifndef DUNGEONCRAFT_APP_LEVEL_H
#define DUNGEONCRAFT_APP_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"

typedef struct dc_level_view dc_level_view_t;

dc_level_view_t *dc_level_view_create(const char *directory, uint64_t seed,
                                      char *err, uint32_t err_cap);
bool dc_level_view_tick(dc_level_view_t *view, char *err, uint32_t err_cap);
bool dc_level_view_wait_visible(dc_level_view_t *view, uint32_t timeout_ms,
                                char *err, uint32_t err_cap);
bool dc_level_view_move(dc_level_view_t *view, int32_t dx, int32_t dy);
bool dc_level_view_paint(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t radius, uint16_t material,
                         char *err, uint32_t err_cap);
bool dc_level_view_pixel(dc_level_view_t *view, uint32_t x, uint32_t y,
                         uint32_t *color, char *err, uint32_t err_cap);
bool dc_level_view_chunk(dc_level_view_t *view, dc_chunk_coord_t coord,
                         dc_chunk_t *chunk, char *err, uint32_t err_cap);
bool dc_level_view_has_chunk(dc_level_view_t *view, dc_chunk_coord_t coord);
bool dc_level_view_destroy(dc_level_view_t *view, char *err, uint32_t err_cap);

#endif
