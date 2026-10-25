#ifndef FR_GAS_ACOUSTIC_GPU_H
#define FR_GAS_ACOUSTIC_GPU_H
#include "body_address_gpu.h"
#include "gpu_internal.h"
#include "renderer_internal.h"
typedef struct {
    fr_renderer_compute_t advect,initialize,rhs,sor,residual,finish;
    dc_gpu_address_buffer_t fields[4];
    bool ready;
} fr_gas_acoustic_gpu_t;
/** Lazily create only the selected optional acoustic module. */
bool fr_gas_acoustic_create(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,
    const VkDescriptorBufferInfo source[11],char *err,uint32_t cap);
/** Release all optional acoustic resources. */
void fr_gas_acoustic_destroy(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module);
/** Complete one staged pressure/advection step, reading reduced residual only. */
bool fr_gas_acoustic_step(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,uint32_t gx,uint32_t gy,
    float seconds,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap);
#endif
