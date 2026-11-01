#include "ferrum/fire/bulk_phase_gpu.h"
bool fr_bulk_phase_gpu_import_chunk(dc_gpu_t *gpu,uint32_t slot,
    const dc_chunk_t *chunk,const fr_bulk_phase_chunk_t *phase,char *err,uint32_t cap) {
    (void)gpu;(void)slot;(void)chunk;(void)phase;(void)err;(void)cap;return false;
}
bool fr_bulk_phase_gpu_export_chunk(dc_gpu_t *gpu,uint32_t slot,
    fr_bulk_phase_chunk_t *out,char *err,uint32_t cap) {
    (void)gpu;(void)slot;(void)out;(void)err;(void)cap;return false;
}
