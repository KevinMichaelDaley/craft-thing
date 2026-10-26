#ifndef FR_GAS_GPU_STATE_H
#define FR_GAS_GPU_STATE_H
#include "dungeoncraft/gas_layout.h"
#include "ferrum/fire/gas_gpu.h"
#include "gas_acoustic_gpu.h"
#include "gas_gpu_internal.h"
typedef struct {float words[FR_GAS_CELL_WORDS];} gas_packet_t;
struct fr_gas_gpu {
    fr_renderer_compute_t initialize,prepare,transport,feedback,gate,audit;
    fr_renderer_compute_t geometry_stage,geometry_transfer,geometry_commit;
    dc_gpu_address_buffer_t state[3],cache[2],parameters,stats,gates,audit_summary;
    dc_gpu_address_buffer_t checkpoint_staging;
    fr_gas_model_t model;
    float density,temperature;
    uint32_t groups_x,groups_y;
    fr_gas_gpu_clock_t clock;
    fr_gas_acoustic_gpu_t acoustic;
    uint32_t policy;
    uint32_t cached_bank;
    float cached_rate;
    bool cache_valid,direct_publish,initialization_pending;
    bool geometry_dirty,geometry_dynamic,geometry_routes_dirty;
    VkFence completion;
    double large_step_hint;
    bool trace;
    uint64_t trace_submits,trace_queue_ticks,trace_wait_ticks;
};
/** Cold completed-state access; device-only stock uses one persistent staging
 * chunk. Never call from simulation kernels or per-frame scheduling. */
bool fr_gas_gpu_checkpoint_packets(dc_gpu_t *gpu,uint32_t bank,uint32_t slot,
    const gas_packet_t **packets,char *err_buf,uint32_t err_cap);
#endif
