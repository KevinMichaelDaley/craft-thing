#ifndef DUNGEONCRAFT_NATIVE_SCENE_H
#define DUNGEONCRAFT_NATIVE_SCENE_H

#include <string.h>

#include "dungeoncraft/chunk.h"

static inline uint32_t native_scene_columns(uint32_t width) {
    return (width + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
}

static inline uint32_t native_scene_rows(uint32_t height) {
    return (height + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
}

static inline uint32_t native_scene_count(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    return 64u;
}

static inline void native_scene_chunk(dc_chunk_t *chunk, uint32_t slot,
                                      uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
    memset(chunk, 0, sizeof(*chunk));
    chunk->coord = (dc_chunk_coord_t){slot % 8u, slot / 8u};
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            dc_cell_t *cell = &chunk->cells[y * DC_CHUNK_SIDE + x];
            cell->material = y == DC_CHUNK_SIDE - 1u ? DC_MATERIAL_STONE : 0;
            cell->fluid_mass = y > 24u && y < 48u ? DC_FLUID_FULL : 0;
        }
}

#endif
