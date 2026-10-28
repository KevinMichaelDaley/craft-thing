#ifndef DC_REVOLUTE_GPU_INTERNAL_H
#define DC_REVOLUTE_GPU_INTERNAL_H
#include "gpu_internal.h"
/** Publish the independently growable joint resource into a new body-work root. */
void fr_gpu_revolute_bind_work(dc_gpu_t *gpu, uint32_t *header);
/** Release joints after the final GPU submission. */
void fr_gpu_revolute_destroy(dc_gpu_t *gpu);
/** Number of allocated joint records dispatched in parallel, including holes. */
uint32_t fr_gpu_revolute_span(const dc_gpu_t *gpu);
/** Lazily prepare joint phases without affecting worlds that have no joints. */
bool dc_gpu_solver_revolute_prepare(dc_gpu_t *gpu, char *err, uint32_t cap);
#endif
