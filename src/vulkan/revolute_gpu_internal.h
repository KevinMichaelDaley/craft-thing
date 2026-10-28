#ifndef DC_REVOLUTE_GPU_INTERNAL_H
#define DC_REVOLUTE_GPU_INTERNAL_H
#include "gpu_internal.h"
#include "structural_store_internal.h"
/** Publish the independently growable joint resource into a new body-work root. */
void fr_gpu_revolute_bind_work(dc_gpu_t *gpu, uint32_t *header);
/** Release joints after the final GPU submission. */
void fr_gpu_revolute_destroy(dc_gpu_t *gpu);
/** Number of allocated joint records dispatched in parallel, including holes. */
uint32_t fr_gpu_revolute_span(const dc_gpu_t *gpu);
/** Lazily prepare joint phases without affecting worlds that have no joints. */
bool dc_gpu_solver_revolute_prepare(dc_gpu_t *gpu, char *err, uint32_t cap);
/** Cold immutable checkpoint channel; prepare all resources before world publication. */
bool fr_gpu_revolute_collect(dc_gpu_t *gpu, uint8_t **bytes, size_t *length,
                             char *err, uint32_t cap);
bool fr_gpu_revolute_prepare(dc_gpu_t *gpu, const uint8_t *bytes, size_t length,
    const dc_structural_store_payload_t *payload, struct fr_revolute_gpu **prepared,
    char *err, uint32_t cap);
void fr_gpu_revolute_discard(dc_gpu_t *gpu, struct fr_revolute_gpu *prepared);
void fr_gpu_revolute_publish(dc_gpu_t *gpu, struct fr_revolute_gpu *prepared);
#endif
