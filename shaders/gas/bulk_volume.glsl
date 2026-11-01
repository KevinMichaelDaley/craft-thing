#ifndef FR_GAS_BULK_VOLUME_GLSL
#define FR_GAS_BULK_VOLUME_GLSL
#include "../../include/dungeoncraft/bulk_phase_layout.h"
layout(set=0,binding=37,std430) readonly buffer BulkPhase {uint bulk_phase[];};
float gas_bulk_volume(uint atlas) {
    uint slot=atlas/4096u,local=atlas%4096u,base=slot*FR_BULK_CHUNK_CHANNELS*4096u;
    uint occupied=min(65536u,bulk_phase[base+local]+bulk_phase[base+4096u+local]);
    return float(occupied)/65536.;
}
#endif
