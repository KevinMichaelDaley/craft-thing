#include "reactive_arena_gpu.h"
bool fr_reactive_arena_create(dc_gpu_t *g,uint32_t initial,fr_reactive_arena_gpu_t *out,char *err,uint32_t cap) {
    (void)g;(void)initial;(void)out;(void)err;(void)cap;return false;
}
void fr_reactive_arena_destroy(dc_gpu_t *g,fr_reactive_arena_gpu_t *arena) {(void)g;(void)arena;}
bool fr_reactive_arena_apply(dc_gpu_t *g,fr_reactive_arena_gpu_t *arena,
    const fr_reactive_arena_request_t *requests,uint32_t count,fr_reactive_handle_t *out,
    uint32_t *required,char *err,uint32_t cap) {
    (void)g;(void)arena;(void)requests;(void)count;(void)out;(void)required;(void)err;(void)cap;return false;
}
bool fr_reactive_arena_read(dc_gpu_t *g,const fr_reactive_arena_gpu_t *arena,
    fr_reactive_handle_t handle,fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    (void)g;(void)arena;(void)handle;(void)out;(void)found;(void)err;(void)cap;return false;
}
bool fr_reactive_arena_stats(dc_gpu_t *g,const fr_reactive_arena_gpu_t *arena,fr_reactive_arena_stats_t *out) {
    (void)g;(void)arena;(void)out;return false;
}
