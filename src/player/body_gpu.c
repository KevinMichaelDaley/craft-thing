#include "ferrum/player/body_gpu.h"

bool fr_beaver_body_admit(dc_gpu_t *g,uint64_t identity,uint64_t transaction,
    uint32_t slot,dc_chunk_coord_t spawn,int32_t x,int32_t y,fr_beaver_body_t *out,
    char *err,uint32_t cap) {
    (void)g;(void)identity;(void)transaction;(void)slot;(void)spawn;
    (void)x;(void)y;(void)out;(void)err;(void)cap;return false;
}
