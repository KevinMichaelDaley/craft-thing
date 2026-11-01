#include <stdio.h>
#include <string.h>

#include "ferrum/fire/bulk_phase_gpu.h"
#include "gas_gpu_internal.h"
#include "gpu_internal.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
bool fr_bulk_phase_gpu_import_chunk(dc_gpu_t *gpu,uint32_t slot,
    const dc_chunk_t *chunk,const fr_bulk_phase_chunk_t *phase,char *err,uint32_t cap) {
    if(!gpu||!gpu->bulk_phase_mapped||slot>=DC_GPU_CHUNK_SLOTS||!chunk||!phase)
        return error(err,cap,"Invalid bulk phase chunk import");
    for(uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if((uint64_t)chunk->cells[i].fluid_mass+phase->oil_fp[i]+phase->slime_fp[i]>
            FR_BULK_CELL_FULL_FP) return error(err,cap,"Bulk phase exceeds shared water capacity");
    if(vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS)
        return error(err,cap,"Cannot complete bulk phase chunk import");
    memcpy((fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped+slot,phase,sizeof(*phase));
    fr_gas_gpu_geometry_changed(gpu,false);
    return true;
}
bool fr_bulk_phase_gpu_export_chunk(dc_gpu_t *gpu,uint32_t slot,
    fr_bulk_phase_chunk_t *out,char *err,uint32_t cap) {
    if(!gpu||!gpu->bulk_phase_mapped||slot>=DC_GPU_CHUNK_SLOTS||!out)
        return error(err,cap,"Invalid bulk phase chunk export");
    if(vkQueueWaitIdle(gpu->queue)!=VK_SUCCESS)
        return error(err,cap,"Cannot complete bulk phase chunk export");
    memcpy(out,(const fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped+slot,sizeof(*out));
    return true;
}
