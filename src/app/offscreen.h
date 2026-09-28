#ifndef DUNGEONCRAFT_APP_OFFSCREEN_H
#define DUNGEONCRAFT_APP_OFFSCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/chunk.h"
#include "dungeoncraft/gpu.h"

typedef struct dc_offscreen dc_offscreen_t;
typedef bool (*dc_offscreen_save_fn)(void *context, const dc_chunk_t *chunk,
                                     char *err, uint32_t cap);

dc_offscreen_t *dc_offscreen_create(dc_gpu_t *parent, dc_chunk_coord_t origin,
                                    char *err, uint32_t cap);
void dc_offscreen_destroy(dc_offscreen_t *offscreen);
bool dc_offscreen_contains(const dc_offscreen_t *offscreen, dc_chunk_coord_t coord);
bool dc_offscreen_can_capture(const dc_offscreen_t *offscreen,
                              dc_chunk_coord_t coord);
bool dc_offscreen_capture(dc_offscreen_t *offscreen, const dc_chunk_t *chunk,
                          char *err, uint32_t cap);
bool dc_offscreen_take(dc_offscreen_t *offscreen, dc_chunk_coord_t coord,
                       dc_chunk_t *chunk, char *err, uint32_t cap);
bool dc_offscreen_has(const dc_offscreen_t *offscreen, dc_chunk_coord_t coord);
bool dc_offscreen_slot(const dc_offscreen_t *offscreen, uint32_t slot,
                       dc_chunk_coord_t *coord, uint32_t *tile_x,
                       uint32_t *tile_y);
uint32_t dc_offscreen_slot_capacity(const dc_offscreen_t *offscreen);
dc_gpu_t *dc_offscreen_gpu(dc_offscreen_t *offscreen);
float dc_offscreen_last_advance_ticks(const dc_offscreen_t *offscreen);
void dc_offscreen_set_spring(dc_offscreen_t *offscreen, bool enabled);
uint32_t dc_offscreen_band(const dc_offscreen_t *offscreen,
                           dc_chunk_coord_t camera_origin);
uint32_t dc_offscreen_coord_band(dc_chunk_coord_t coord,
                                 dc_chunk_coord_t camera_origin);
bool dc_offscreen_update(dc_offscreen_t *offscreen, dc_chunk_coord_t camera_origin,
                         double elapsed_seconds, bool catch_up,
                         char *err, uint32_t cap);
bool dc_offscreen_flush(dc_offscreen_t *offscreen, dc_offscreen_save_fn save,
                        void *context, char *err, uint32_t cap);

#endif
