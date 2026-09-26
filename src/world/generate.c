#include <stdbool.h>
#include <string.h>

#include "dungeoncraft/generate.h"

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

static uint32_t positive_mod(int64_t value, uint32_t divisor) {
    int64_t remainder = value % (int64_t)divisor;
    return (uint32_t)(remainder < 0 ? remainder + divisor : remainder);
}

void dc_generate_chunk(uint64_t seed, dc_chunk_coord_t coord, dc_chunk_t *chunk) {
    if (!chunk) return;
    memset(chunk, 0, sizeof(*chunk));
    chunk->coord = coord;
    uint32_t tile_x_512 = positive_mod(coord.x, 8) * DC_CHUNK_SIDE;
    uint32_t tile_x_256 = positive_mod(coord.x, 4) * DC_CHUNK_SIDE;
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
        uint64_t anchor = (uint64_t)coord.x * 2u + x / 32u;
        uint32_t offset = x % 32u;
        uint32_t left = 24u + (uint32_t)(mix64(seed ^ anchor) % 17u);
        uint32_t right = 24u + (uint32_t)(mix64(seed ^ (anchor + 1u)) % 17u);
        uint32_t height = (left * (32u - offset) + right * offset + 16u) / 32u;
        uint32_t basin_x = tile_x_512 + x;
        uint32_t cave_x = tile_x_256 + x;
        for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
            dc_cell_t *cell = &chunk->cells[y * DC_CHUNK_SIDE + x];
            bool solid = coord.y > 0 || (coord.y == 0 && y >= height);
            if (coord.y == 0 && basin_x > 112 && basin_x < 144 &&
                y >= 30 && y <= 52) {
                solid = false;
                if (y >= 36) cell->fluid_mass = DC_FLUID_FULL;
            }
            if (coord.y == 1) {
                int32_t dx = (int32_t)cave_x - 128;
                int32_t dy = (int32_t)y - 6;
                if (dx * dx + dy * dy < 18 * 18) solid = false;
            }
            cell->material = solid ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
        }
    }
}
