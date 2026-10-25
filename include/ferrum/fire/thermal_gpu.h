#ifndef FERRUM_FIRE_THERMAL_GPU_H
#define FERRUM_FIRE_THERMAL_GPU_H
#include "dungeoncraft/gpu.h"
#include "ferrum/fire/thermal.h"

/** Enable the thermal module without changing physical resolution or clocks. */
bool fr_thermal_gpu_enable(dc_gpu_t *gpu,char *err_buf,uint32_t err_cap);
/** Explicit standalone thermal interval for reference/diagnostic fixtures.
 * Gameplay records the same module once in its normal accepted physical tick. */
bool fr_thermal_gpu_step(dc_gpu_t *gpu,float seconds,char *err_buf,uint32_t err_cap);
#endif
