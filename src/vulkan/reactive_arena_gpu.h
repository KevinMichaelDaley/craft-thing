#ifndef FR_REACTIVE_ARENA_GPU_H
#define FR_REACTIVE_ARENA_GPU_H
#include "body_address_gpu.h"
#include "renderer_internal.h"
#include "ferrum/fire/reactive_state.h"

typedef struct {uint32_t index,generation;} fr_reactive_handle_t;
enum {FR_REACTIVE_ARENA_ALLOCATE=1u,FR_REACTIVE_ARENA_UPDATE=2u,FR_REACTIVE_ARENA_RETIRE=3u};
typedef struct {
    uint32_t operation;
    fr_reactive_handle_t handle;
    uint32_t reserved;
    fr_reactive_record_t record;
} fr_reactive_arena_request_t;
typedef struct {uint32_t capacity,live,free,retired;} fr_reactive_arena_stats_t;
typedef struct {
    dc_gpu_address_buffer_t root,entries,free_bits,mailbox,requests;
    fr_renderer_compute_t compute;
    uint32_t capacity,request_capacity;
} fr_reactive_arena_gpu_t;

/** Create an allocation quantum, never a permanent parcel quota. */
bool fr_reactive_arena_create(dc_gpu_t *gpu,uint32_t initial_capacity,
    fr_reactive_arena_gpu_t *out,char *err,uint32_t cap);
/** Destroy resources only after their final GPU use. */
void fr_reactive_arena_destroy(dc_gpu_t *gpu,fr_reactive_arena_gpu_t *arena);
/** Cold import/diagnostic boundary. Reserve, publish or roll back the entire
 * batch on GPU. Resource pressure grows storage and retries; allocation failure
 * leaves live records and output handles unchanged. Runtime producers use the
 * GPU primitives directly, without constructing or reading CPU parcel lists. */
bool fr_reactive_arena_apply(dc_gpu_t *gpu,fr_reactive_arena_gpu_t *arena,
    const fr_reactive_arena_request_t *requests,uint32_t count,
    fr_reactive_handle_t *out,uint32_t *required_capacity,char *err,uint32_t cap);
/** Completed-state diagnostics, never runtime physics readbacks. */
bool fr_reactive_arena_read(dc_gpu_t *gpu,const fr_reactive_arena_gpu_t *arena,
    fr_reactive_handle_t handle,fr_reactive_record_t *out,bool *found,char *err,uint32_t cap);
/** Read constant-size allocation counters at a completed boundary. */
bool fr_reactive_arena_stats(dc_gpu_t *gpu,const fr_reactive_arena_gpu_t *arena,
    fr_reactive_arena_stats_t *out);
/** Preflight a bounded GPU producer using allocation counters only. Grow by
 * device copies before mutation; never read or construct a CPU parcel list. */
bool fr_reactive_arena_reserve_free(dc_gpu_t *gpu,fr_reactive_arena_gpu_t *arena,
    uint32_t count,char *err,uint32_t cap);
#endif
