#ifndef FR_REACTIVE_STORAGE_GPU_H
#define FR_REACTIVE_STORAGE_GPU_H
#include "reactive_arena_gpu.h"

struct fr_reactive_gpu {
    fr_reactive_arena_gpu_t arena;
    dc_gpu_address_buffer_t cells,shapes,particles;
    dc_gpu_address_buffer_t stream,stream_marks,snapshot;
    fr_renderer_compute_t stream_compute;
    uint64_t high_id;
};
/** Lazy cold-boundary resource creation. */
bool fr_gpu_reactive_initialize(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Preflight canonical and alternate particle handles before admission. */
bool fr_gpu_reactive_particle_storage(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Preflight detached storage without changing the live work root. */
bool fr_gpu_reactive_storage_create(dc_gpu_t *gpu,uint32_t shape_words,
    struct fr_reactive_gpu **out,char *err,uint32_t cap);
/** Destroy detached or retired storage after completed GPU use. */
void fr_gpu_reactive_storage_destroy(dc_gpu_t *gpu,struct fr_reactive_gpu *storage);
/** Preflight particle handles in detached storage before publication. */
bool fr_gpu_reactive_storage_particles(dc_gpu_t *gpu,struct fr_reactive_gpu *storage,
    char *err,uint32_t cap);
/** Cold GPU compaction of logical BODY records, without a host ownership walk. */
bool fr_gpu_reactive_export_bodies(dc_gpu_t *gpu,fr_reactive_record_t **records,
    uint32_t *count,uint64_t *next_id,char *err,uint32_t cap);
#endif
