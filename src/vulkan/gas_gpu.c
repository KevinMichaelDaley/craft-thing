#include <stdio.h>
#include "ferrum/fire/gas_gpu.h"

static bool pending(char *err,uint32_t cap) {
    if(err&&cap) snprintf(err,cap,"Vulkan gas module is not implemented");
    return false;
}
bool fr_gas_gpu_enable(dc_gpu_t *g,const fr_gas_model_t *m,float density,float temperature,char *err,uint32_t cap) {
    (void)g;(void)m;(void)density;(void)temperature;return pending(err,cap);
}
bool fr_gas_gpu_step(dc_gpu_t *g,float seconds,char *err,uint32_t cap) {
    (void)g;(void)seconds;return pending(err,cap);
}
bool fr_gas_gpu_seed_chunk(dc_gpu_t *g,uint32_t slot,const fr_gas_state_t *s,
    const double *v,uint32_t count,char *err,uint32_t cap) {
    (void)g;(void)slot;(void)s;(void)v;(void)count;return pending(err,cap);
}
bool fr_gas_gpu_read_chunk(dc_gpu_t *g,uint32_t slot,fr_gas_state_t *s,uint32_t count,char *err,uint32_t cap) {
    (void)g;(void)slot;(void)s;(void)count;return pending(err,cap);
}
bool fr_gas_gpu_clock(const dc_gpu_t *g,fr_gas_gpu_clock_t *out) {(void)g;(void)out;return false;}
