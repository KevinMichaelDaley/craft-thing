#include "ferrum/fire/gpu.h"

bool fr_gpu_reactive_seed_cell(dc_gpu_t *g,uint32_t slot,uint32_t local,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    (void)g;(void)slot;(void)local;(void)state;(void)err;(void)cap;return false;
}
bool fr_gpu_reactive_read_cell(dc_gpu_t *g,uint32_t slot,uint32_t local,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    (void)g;(void)slot;(void)local;(void)out;(void)found;(void)err;(void)cap;return false;
}
