#ifndef DUNGEONCRAFT_GENERATE_H
#define DUNGEONCRAFT_GENERATE_H

#include <stdint.h>
#include <stdbool.h>

#include "dungeoncraft/chunk.h"

typedef struct {
    int64_t origin_x, origin_y;
    uint32_t width, soil_depth;
    const uint32_t *surface;
} dc_ground_profile_t;

/** Generate cold terrain against an authored ground line; extend edge heights
 * outside its footprint. The profile is immutable; no runtime physics is solved.
 */
bool dc_generate_chunk_ground(dc_chunk_coord_t coord, const dc_ground_profile_t *ground,
                               dc_chunk_t *chunk);

/** Generate one deterministic world chunk from a seed and signed coordinates. */
void dc_generate_chunk(uint64_t seed, dc_chunk_coord_t coord, dc_chunk_t *chunk);

/** Generate terrain at an integer world scale in simulated pixels. */
bool dc_generate_chunk_scaled(uint64_t seed, dc_chunk_coord_t coord,
                              uint32_t scale, dc_chunk_t *chunk);

#endif
