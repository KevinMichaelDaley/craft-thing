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
    fr_renderer_compute_t activity_cached;
    fr_renderer_compute_t transport_factor,transport_prefix,transport_apply,transport_metadata;
    fr_renderer_compute_t restrict_pressure,coarse_sor,prolong_pressure;
    fr_renderer_compute_t stage_transport;
    fr_renderer_compute_t practical_factor,practical_prefix,practical_apply;
    fr_renderer_compute_t practical_axis;
    fr_renderer_compute_t practical_explicit;
    fr_renderer_compute_t practical_hybrid;
    fr_renderer_compute_t practical_hybrid_axis[2];
    uint32_t hybrid_line_group;
    fr_renderer_compute_t temporal_sor,sor_tile16,group_tile16;
    fr_renderer_compute_t coarse_group;
    fr_renderer_compute_t mg_relax[2],mg_restrict[3],mg_prolong[3];
    fr_renderer_compute_t mg_temporal[2];
    fr_renderer_compute_t rhs_fused;
    fr_renderer_compute_t metadata_lean,rhs_lean;
    fr_renderer_compute_t pack_pressure,packed_sor;
    fr_renderer_compute_t reset_pressure_mask;
    fr_renderer_compute_t mg_reconcile[3];
    fr_renderer_compute_t pack_half_pressure,half_sor;
    fr_renderer_compute_t finish_group[2];
    dc_gpu_address_buffer_t transport_work;
    VkBuffer state_bank[2];VkDeviceSize state_bytes;
    dc_gpu_address_buffer_t fields[11],indirect;
    bool ready,matrix_valid,matrix_large,large_transport,supports_large_transport,practical;
    bool direct_first_axis,geometry_changed;
    bool sparse_owners;
} fr_gas_acoustic_gpu_t;
/** Lazily create only the selected optional acoustic module. */
bool fr_gas_acoustic_create(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,
    const VkDescriptorBufferInfo source[12],VkDescriptorBufferInfo owners,char *err,uint32_t cap);
/** Release all optional acoustic resources. */
void fr_gas_acoustic_destroy(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module);
/** Complete one staged pressure/advection step, reading reduced residual only. */
bool fr_gas_acoustic_step(dc_gpu_t *gpu,fr_gas_acoustic_gpu_t *module,uint32_t gx,uint32_t gy,
    float seconds,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap);
#endif
