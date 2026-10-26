#ifndef FERRUM_FIRE_COMBUSTION_GPU_H
#define FERRUM_FIRE_COMBUSTION_GPU_H
#include "dungeoncraft/gpu.h"
/** Enable ordinary terrain combustion against existing thermal and gas owners.
 * This reserves pipelines only; it never creates fuel, oxygen or ignition. */
bool fr_combustion_gpu_enable(dc_gpu_t *gpu,char *err_buf,uint32_t err_cap);
/** Standalone diagnostic interval using the production GPU transaction.
 * Gameplay records it once in the accepted physical tick. */
bool fr_combustion_gpu_step(dc_gpu_t *gpu,float seconds,char *err_buf,uint32_t err_cap);
#endif
