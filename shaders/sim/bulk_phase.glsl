#ifndef DC_SIM_BULK_PHASE_GLSL
#define DC_SIM_BULK_PHASE_GLSL
#include "../../include/dungeoncraft/bulk_phase_layout.h"
layout(set=0,binding=37,std430) readonly buffer BulkPhase {uint bulk_phase[];};
uint bulk_occupied(ivec2 p,uint page) {
    uint local=uint(p.y%64)*64u+uint(p.x%64);
    uint base=(page-1u)*FR_BULK_CHUNK_CHANNELS*4096u;
    return min(FULL,bulk_phase[base+local]+bulk_phase[base+4096u+local]);
}
uint bulk_water_capacity(ivec2 p,uint page,uint rigid_occupied) {
    return FULL-min(FULL,min(FULL,rigid_occupied)+bulk_occupied(p,page));
}
#endif
