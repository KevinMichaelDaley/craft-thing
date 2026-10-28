#ifndef DC_AUTHORED_RIGID_GPU_INTERNAL_H
#define DC_AUTHORED_RIGID_GPU_INTERNAL_H
#include "gpu_internal.h"
/** Bind cold immutable object tags into a newly grown body-work resource. */
void fr_gpu_authored_objects_bind(dc_gpu_t *gpu,uint32_t *header);
/** Lazily compile only object-aware extraction phases; ordinary kernels stay specialized off. */
bool dc_gpu_component_objects_prepare(dc_gpu_t *gpu,char *err,uint32_t cap);
/** Submit one cold parallel batch; caller services further resource/progress demand. */
bool dc_gpu_admit_authored_rigid_batch(dc_gpu_t *gpu,char *err,uint32_t cap);
#endif
