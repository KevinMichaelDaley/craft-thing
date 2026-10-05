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
    return native_scene_columns(width) * native_scene_rows(height);
}

static inline void native_scene_chunk(dc_chunk_t *chunk, uint32_t slot,
                                      uint32_t width, uint32_t height) {
    memset(chunk, 0, sizeof(*chunk));
    uint32_t columns = native_scene_columns(width);
    chunk->coord = (dc_chunk_coord_t){slot % columns, slot / columns};
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            dc_cell_t *cell = &chunk->cells[y * DC_CHUNK_SIDE + x];
            uint32_t world_x = (uint32_t)chunk->coord.x * DC_CHUNK_SIDE + x;
            uint32_t world_y = (uint32_t)chunk->coord.y * DC_CHUNK_SIDE + y;
            if (world_x >= width || world_y >= height) continue;
            if (world_y == height - 1u || world_x == 0u || world_x == width - 1u)
                cell->material = DC_MATERIAL_STONE;
            else if (world_y >= height * 4u / 5u && world_y < height * 4u / 5u + 16u &&
                     world_x >= width / 4u && world_x < width * 3u / 4u)
                cell->material = DC_MATERIAL_SAND;
            if (cell->material != DC_MATERIAL_STONE && world_y >= height / 2u)
                cell->fluid_mass = DC_FLUID_FULL;
        }
    dc_chunk_seed_particles(chunk);
}

#endif
