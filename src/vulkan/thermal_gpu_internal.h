#ifndef FR_THERMAL_GPU_INTERNAL_H
#define FR_THERMAL_GPU_INTERNAL_H
#include "gpu_internal.h"
/** Record one accepted physical interval; no work or allocation when disabled. */
void fr_thermal_record_step(dc_gpu_t *gpu,float seconds);
/** Refresh and render thermal feedback without changing physical state. */
void fr_thermal_record_overlay(dc_gpu_t *gpu);
/** Release thermal resources before their reactive backing storage. */
void fr_thermal_gpu_destroy(dc_gpu_t *gpu);
/** Cold physical-slot replacement after validation and completed upload. */
void fr_thermal_clear_water_chunk(dc_gpu_t *gpu,uint32_t slot);
#endif
