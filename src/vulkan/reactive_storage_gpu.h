#ifndef FR_REACTIVE_STORAGE_GPU_H
#define FR_REACTIVE_STORAGE_GPU_H
#include "reactive_arena_gpu.h"

struct fr_reactive_gpu {
    fr_reactive_arena_gpu_t arena;
    dc_gpu_address_buffer_t cells,shapes,particles;
    dc_gpu_address_buffer_t stream,stream_marks;
    fr_renderer_compute_t stream_compute;
    uint64_t high_id;
};
/** Lazy cold-boundary resource creation. */
bool fr_gpu_reactive_initialize(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Preflight canonical and alternate particle handles before admission. */
bool fr_gpu_reactive_particle_storage(dc_gpu_t *gpu,char *err,uint32_t cap);
#endif
