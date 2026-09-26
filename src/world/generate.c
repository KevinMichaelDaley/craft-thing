#include <string.h>

#include "dungeoncraft/generate.h"

void dc_generate_chunk(uint64_t seed, dc_chunk_coord_t coord, dc_chunk_t *chunk) {
    (void)seed; (void)coord;
    memset(chunk, 0, sizeof(*chunk));
}
