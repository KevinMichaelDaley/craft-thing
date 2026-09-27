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

bool dc_generate_chunk_scaled(uint64_t seed, dc_chunk_coord_t coord,
                              uint32_t scale, dc_chunk_t *chunk) {
    if (!chunk || !scale || DC_CHUNK_SIDE % scale != 0u) return false;
    memset(chunk, 0, sizeof(*chunk));
    chunk->coord = coord;
    uint32_t chunk_sub_x = positive_mod(coord.x, scale);
    uint32_t chunk_sub_y = positive_mod(coord.y, scale);
    int64_t coarse_chunk_x = (coord.x - (int64_t)chunk_sub_x) / (int64_t)scale;
    int64_t coarse_chunk_y = (coord.y - (int64_t)chunk_sub_y) / (int64_t)scale;
    uint32_t coarse_stride = DC_CHUNK_SIDE / scale;
    uint32_t tile_x_512 = positive_mod(coarse_chunk_x, 8) * DC_CHUNK_SIDE;
    uint32_t tile_x_256 = positive_mod(coarse_chunk_x, 4) * DC_CHUNK_SIDE;
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
        uint32_t coarse_x = chunk_sub_x * coarse_stride + x / scale;
        uint64_t anchor = (uint64_t)coarse_chunk_x * 2u + coarse_x / 32u;
        uint32_t offset = coarse_x % 32u;
        uint32_t left = 24u + (uint32_t)(mix64(seed ^ anchor) % 17u);
        uint32_t right = 24u + (uint32_t)(mix64(seed ^ (anchor + 1u)) % 17u);
        uint32_t height = (left * (32u - offset) + right * offset + 16u) / 32u;
        uint32_t basin_x = tile_x_512 + coarse_x;
        uint32_t cave_x = tile_x_256 + coarse_x;
        for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
            uint32_t coarse_y = chunk_sub_y * coarse_stride + y / scale;
            dc_cell_t *cell = &chunk->cells[y * DC_CHUNK_SIDE + x];
            bool solid = coarse_chunk_y > 0 ||
                         (coarse_chunk_y == 0 && coarse_y >= height);
            if (coarse_chunk_y == 0 && basin_x > 112 && basin_x < 144 &&
                coarse_y >= 30 && coarse_y <= 52) {
                solid = false;
                if (coarse_y >= 36) cell->fluid_mass = DC_FLUID_FULL;
            }
            if (coarse_chunk_y == 1) {
                int32_t dx = (int32_t)cave_x - 128;
                int32_t dy = (int32_t)coarse_y - 6;
                if (dx * dx + dy * dy < 18 * 18) solid = false;
            }
            cell->material = solid ? DC_MATERIAL_STONE : DC_MATERIAL_AIR;
            if (solid && coarse_chunk_y == 0 && coarse_y < height + 2u &&
                (mix64(seed ^ (uint64_t)coarse_chunk_x * 19u ^
                       (uint64_t)coarse_x / 12u) & 3u) == 0u)
                cell->material = DC_MATERIAL_SAND;
        }
    }
    dc_chunk_seed_particles(chunk);
    return true;
}

void dc_generate_chunk(uint64_t seed, dc_chunk_coord_t coord, dc_chunk_t *chunk) {
    (void)dc_generate_chunk_scaled(seed, coord, 1u, chunk);
}
