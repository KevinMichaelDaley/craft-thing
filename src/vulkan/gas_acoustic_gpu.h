#ifndef FR_GAS_ACOUSTIC_GPU_H
#define FR_GAS_ACOUSTIC_GPU_H
#include "body_address_gpu.h"
#include "gpu_internal.h"
#include "renderer_internal.h"
typedef struct {
    fr_renderer_compute_t advect,initialize,rhs,classify,compact,sor,correct,residual,finish;
    fr_renderer_compute_t activity,activity_compact,final_compact,commit;
    fr_renderer_compute_t clear_pressure,prepare_pressure;
    fr_renderer_compute_t activity_candidates;
    fr_renderer_compute_t transport_factor,transport_prefix,transport_apply,transport_metadata;
    fr_renderer_compute_t restrict_pressure,coarse_sor,prolong_pressure;
    fr_renderer_compute_t stage_transport;
    dc_gpu_address_buffer_t transport_work;
    VkBuffer state_bank[2];VkDeviceSize state_bytes;
    dc_gpu_address_buffer_t fields[11],indirect;
    bool ready,matrix_valid,matrix_large,large_transport,supports_large_transport,practical;
} fr_gas_acoustic_gpu_t;
/** Lazily create only the selected optional acoustic module. */
bool fr_gas_acoustic_create(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,
    const VkDescriptorBufferInfo source[12],char *err,uint32_t cap);
/** Release all optional acoustic resources. */
void fr_gas_acoustic_destroy(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module);
/** Complete one staged pressure/advection step, reading reduced residual only. */
bool fr_gas_acoustic_step(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,uint32_t gx,uint32_t gy,
    float seconds,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap);
#endif
