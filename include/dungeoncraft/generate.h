#ifndef DUNGEONCRAFT_GENERATE_H
#define DUNGEONCRAFT_GENERATE_H

#include <stdint.h>

#include "dungeoncraft/chunk.h"

/** Generate one deterministic world chunk from a seed and signed coordinates. */
void dc_generate_chunk(uint64_t seed, dc_chunk_coord_t coord, dc_chunk_t *chunk);

#endif
