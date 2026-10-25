#ifndef FERRUM_FIRE_GPU_H
#define FERRUM_FIRE_GPU_H
#include "dungeoncraft/gpu.h"
#include "ferrum/fire/reactive_state.h"

/** Cold fixture/import boundary only: attach validated state to a resident cell.
 * Runtime reaction and mechanical movement never call this CPU admission API.
 * New IDs must exceed the context's high water mark; existing IDs may update
 * their same owner. Failure preserves the previous state and owner. */
bool fr_gpu_reactive_seed_cell(dc_gpu_t *gpu,uint32_t slot,uint32_t local_cell,
    const fr_reactive_state_t *state,char *err_buf,uint32_t err_cap);
/** Explicit completed-queue diagnostic. Missing state sets found=false without
 * changing out; this is not a runtime thermal or component readback. */
bool fr_gpu_reactive_read_cell(dc_gpu_t *gpu,uint32_t slot,uint32_t local_cell,
    fr_reactive_record_t *out,bool *found,char *err_buf,uint32_t err_cap);
/** Explicit diagnostic by stable body identity and canonical rest-grid texel. */
bool fr_gpu_reactive_read_body(dc_gpu_t *gpu,uint32_t body_id,uint32_t x,uint32_t y,
    fr_reactive_record_t *out,bool *found,char *err_buf,uint32_t err_cap);
/** Cold import for one existing loose particle in the canonical A pool. */
bool fr_gpu_reactive_seed_particle(dc_gpu_t *gpu,uint32_t particle,
    const fr_reactive_state_t *state,char *err_buf,uint32_t err_cap);
/** Explicit completed-queue canonical-particle diagnostic. */
bool fr_gpu_reactive_read_particle(dc_gpu_t *gpu,uint32_t particle,
    fr_reactive_record_t *out,bool *found,char *err_buf,uint32_t err_cap);
#endif
