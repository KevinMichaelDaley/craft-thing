#ifndef FR_REACTIVE_GPU_INTERNAL_H
#define FR_REACTIVE_GPU_INTERNAL_H
#include "body_address_gpu.h"
#include "ferrum/fire/gpu.h"

/** Publish the lazy root into a newly allocated body work header. */
void fr_gpu_reactive_bind_work(dc_gpu_t *gpu,uint32_t *header);
/** Release reactive resources after the device is idle. */
void fr_gpu_reactive_destroy(dc_gpu_t *gpu);
#endif
