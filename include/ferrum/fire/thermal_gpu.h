#ifndef FERRUM_FIRE_THERMAL_GPU_H
#define FERRUM_FIRE_THERMAL_GPU_H
#include "dungeoncraft/gpu.h"
#include "ferrum/fire/thermal.h"

/** Enable the thermal module without changing physical resolution or clocks. */
bool fr_thermal_gpu_enable(dc_gpu_t *gpu,char *err_buf,uint32_t err_cap);
/** Explicit standalone thermal interval for reference/diagnostic fixtures.
 * Gameplay records the same module once in its normal accepted physical tick. */
bool fr_thermal_gpu_step(dc_gpu_t *gpu,float seconds,char *err_buf,uint32_t err_cap);
/** Set finite probe contact using physical-grid coordinates; no state readback. */
bool fr_thermal_gpu_probe(dc_gpu_t *gpu,uint32_t x,uint32_t y,bool active);
/** Choose authored color, temperature or wetness feedback independently of physics. */
bool fr_thermal_gpu_overlay(dc_gpu_t *gpu,uint32_t mode);
typedef struct {
    uint32_t occupied,material;
    float thermal_energy,heat_capacity,temperature_K,wetness;
} fr_thermal_inspection_t;
/** Completed-queue single-point diagnostic. Gameplay hover feedback reads the
 * same GPU cache directly; this API must never become a per-frame CPU solve. */
bool fr_thermal_gpu_inspect(dc_gpu_t *gpu,uint32_t x,uint32_t y,
    fr_thermal_inspection_t *out,char *err_buf,uint32_t err_cap);
/** Completed-queue finite-source snapshot for diagnostics/checkpoints only. */
bool fr_thermal_gpu_source_state(dc_gpu_t *gpu,fr_thermal_probe_t *out,
    char *err_buf,uint32_t err_cap);
/** Cold fixture/import of liquid enthalpy above ambient, independent of pigment
 * and solid parcel state. Validate against actual authoritative water volume. */
bool fr_thermal_gpu_seed_water(dc_gpu_t *gpu,uint32_t slot,uint32_t local_cell,
    float excess_energy,char *err_buf,uint32_t err_cap);
/** Completed-queue diagnostic/save snapshot, never a runtime CPU fluid solve. */
bool fr_thermal_gpu_read_water_chunk(dc_gpu_t *gpu,uint32_t slot,float *excess_energy,
    uint32_t cell_capacity,char *err_buf,uint32_t err_cap);
#endif
