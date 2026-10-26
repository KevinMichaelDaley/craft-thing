#ifndef FERRUM_FIRE_GAS_GPU_H
#define FERRUM_FIRE_GAS_GPU_H
#include "dungeoncraft/gpu.h"
#include "dungeoncraft/gas_audit_layout.h"
#include "ferrum/fire/gas.h"
#include "ferrum/fire/gas_checkpoint.h"

typedef struct {float metrics[FR_GAS_AUDIT_WORDS];} fr_gas_gpu_audit_t;
/** Cold scalar diagnostic that a completed hybrid line reused its rate bound. */
bool fr_gas_gpu_reused_line_rate(dc_gpu_t *gpu,bool *used);
/** Cold scalar diagnostic of the completed practical energy-update backend. */
bool fr_gas_gpu_paired_energy(dc_gpu_t *gpu,bool *used);
/** Cold GPU reduction after queue completion; reads only32 scalar diagnostics.
 * Never invoked by normal simulation frames and never advances physical time. */
bool fr_gas_gpu_audit(dc_gpu_t *gpu,fr_gas_gpu_audit_t *out,char *err_buf,uint32_t err_cap);
enum {FR_GAS_PRESSURE_HLLC,FR_GAS_PRESSURE_IMPLICIT,FR_GAS_PRESSURE_LARGE_CFL,FR_GAS_PRESSURE_PRACTICAL};
/** Enable one authoritative Vulkan gas field. Coefficients and initial ambient
 * stock are explicit; repeated configuration must not replenish existing gas. */
bool fr_gas_gpu_enable(dc_gpu_t *gpu,const fr_gas_model_t *model,
    float ambient_density,float ambient_K,char *err_buf,uint32_t err_cap);
/** Advance the complete requested interval using reference-validated CFL steps.
 * Runtime reads only reduced scalar scheduling metadata, never field lists. */
bool fr_gas_gpu_step(dc_gpu_t *gpu,float seconds,char *err_buf,uint32_t err_cap);
/** Configure reference-atmosphere gravity in cells per physical second squared. */
bool fr_gas_gpu_set_gravity(dc_gpu_t *gpu,float gravity,char *err_buf,uint32_t err_cap);
/** Select a validated acoustic policy without changing stock or physical time. */
bool fr_gas_gpu_set_pressure_policy(dc_gpu_t *gpu,uint32_t policy,char *err_buf,uint32_t err_cap);
/** Open/close gas-flow faces in a physical-cell rectangle. Axis0 is the right
 * face, axis1 the lower face. This changes boundary conditions, never gas stock. */
bool fr_gas_gpu_gate(dc_gpu_t *gpu,uint32_t x,uint32_t y,uint32_t width,uint32_t height,
    uint32_t axis,bool closed,char *err_buf,uint32_t err_cap);
/** Read-only feedback:0 smoke pigment,1 oxygen molecular fraction. */
bool fr_gas_gpu_overlay(dc_gpu_t *gpu,uint32_t mode);
/** Cold work diagnostic, never a frame scan. Practical mode also records
 * mechanical changes from transport and pressure projection. */
bool fr_gas_gpu_read_work_chunk(dc_gpu_t *gpu,uint32_t slot,double *work,
    uint32_t capacity,char *err_buf,uint32_t err_cap);
/** Completed-queue fixture/import of immutable logical chunk data. */
bool fr_gas_gpu_seed_chunk(dc_gpu_t *gpu,uint32_t slot,const fr_gas_state_t *states,
    const double *void_fractions,uint32_t count,char *err_buf,uint32_t err_cap);
/** Cold diagnostic/checkpoint snapshot; never a per-frame CPU gas solver. */
bool fr_gas_gpu_read_chunk(dc_gpu_t *gpu,uint32_t slot,fr_gas_state_t *states,
    uint32_t capacity,char *err_buf,uint32_t err_cap);
/** Read accepted clock counters without reading simulation cells. */
bool fr_gas_gpu_clock(const dc_gpu_t *gpu,fr_gas_gpu_clock_t *out);
/** Cold completed-queue export; never part of the simulation loop. */
bool fr_gas_gpu_export_checkpoint(dc_gpu_t *gpu,uint32_t slot,
    fr_gas_checkpoint_chunk_t *out,char *err_buf,uint32_t err_cap);
/** Validate the complete immutable checkpoint before publishing any stock. */
bool fr_gas_gpu_import_checkpoint(dc_gpu_t *gpu,uint32_t slot,
    const fr_gas_checkpoint_chunk_t *input,char *err_buf,uint32_t err_cap);
/** Mark a genuinely new streamed chunk for GPU ambient initialization. Never
 * use for a restored chunk; admission metadata is independent of cell stock. */
bool fr_gas_gpu_queue_ambient_chunk(dc_gpu_t *gpu,uint32_t slot,char *err_buf,uint32_t err_cap);
/** Initialize all marked uploaded chunks in one cold GPU transaction, including
 * offscreen slots. Existing stock and physical clock are untouched. */
bool fr_gas_gpu_initialize_queued(dc_gpu_t *gpu,char *err_buf,uint32_t err_cap);
#endif
