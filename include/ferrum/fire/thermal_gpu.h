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
#endif
