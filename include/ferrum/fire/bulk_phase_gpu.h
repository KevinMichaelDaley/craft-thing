#ifndef FERRUM_FIRE_BULK_PHASE_GPU_H
#define FERRUM_FIRE_BULK_PHASE_GPU_H

#include "dungeoncraft/gpu.h"
#include "ferrum/fire/bulk_phase.h"

/** Cold admission of authoritative oil/slime volume and retained overflow. */
bool fr_bulk_phase_gpu_import_chunk(dc_gpu_t *gpu,uint32_t slot,
    const dc_chunk_t *chunk,const fr_bulk_phase_chunk_t *phase,
    char *err_buf,uint32_t err_cap);
/** Cold readback for checkpointing and conservation audits. */
bool fr_bulk_phase_gpu_export_chunk(dc_gpu_t *gpu,uint32_t slot,
    fr_bulk_phase_chunk_t *out,char *err_buf,uint32_t err_cap);
/** Advance conservative bulk phase face exchange on resident GPU cells. */
bool fr_bulk_phase_gpu_step(dc_gpu_t *gpu,float seconds,
    char *err_buf,uint32_t err_cap);

#endif
