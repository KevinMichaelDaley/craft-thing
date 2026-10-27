#ifndef FERRUM_FIRE_COMBUSTION_GPU_H
#define FERRUM_FIRE_COMBUSTION_GPU_H
#include "dungeoncraft/gpu.h"
/** Cold bounded diagnostic: copies14 GPU counters for authored bonfire progress
 * and selected BODY exposure. Does not read parcel, cell or component lists. */
bool fr_combustion_gpu_bonfire_counters(dc_gpu_t *gpu,uint32_t out[14]);
/** Enable readable bonfire presentation feedback without CPU field readback. */
bool fr_combustion_gpu_hud(dc_gpu_t *gpu,bool enabled,bool wet,bool paused,uint32_t x,uint32_t y);
/** Enable ordinary terrain combustion against existing thermal and gas owners.
 * This reserves pipelines only; it never creates fuel, oxygen or ignition. */
bool fr_combustion_gpu_enable(dc_gpu_t *gpu,char *err_buf,uint32_t err_cap);
/** Query module configuration without reading simulation state. */
bool fr_combustion_gpu_enabled(const dc_gpu_t *gpu);
/** Standalone diagnostic interval using the production GPU transaction.
 * Gameplay records it once in the accepted physical tick. */
bool fr_combustion_gpu_step(dc_gpu_t *gpu,float seconds,char *err_buf,uint32_t err_cap);
/** Derived Eulerian source output, not another mass or energy inventory. */
typedef struct {float power,temperature_K;uint32_t generation,sources;} fr_combustion_emission_t;
/** Completed-queue diagnostic of admitted reaction power. Never used for a
 * runtime CPU field solve or per-flame light list. */
bool fr_combustion_gpu_read_emission_chunk(dc_gpu_t *gpu,uint32_t slot,
    fr_combustion_emission_t *out,uint32_t capacity,char *err_buf,uint32_t err_cap);
#endif
