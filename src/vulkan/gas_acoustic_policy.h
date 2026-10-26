#ifndef FR_GAS_ACOUSTIC_POLICY_H
#define FR_GAS_ACOUSTIC_POLICY_H
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gas_layout.h"
#include "gas_acoustic_gpu.h"

/** Admit exact temporal blocking only for supported half-row fine sweeps. */
static inline bool fr_gas_acoustic_large_sor_enabled(const fr_gas_acoustic_gpu_t *t,uint32_t bank) {
    const char *setting=getenv("DC_GAS_SOR_TILE");
    return t->sor_tiled.pipeline&&setting&&strcmp(setting,"0")!=0&&(bank&FR_GAS_HALF_PRESSURE_BIT)!=0u&&
        (bank&(FR_GAS_MG_BOUNDARY_BIT|FR_GAS_COMPARE_ROW_SETUP_BIT))==0u;
}
/** Admit the bounded hierarchy only within its validated interval and scratch range. */
static inline bool fr_gas_acoustic_multigrid_enabled(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,float dt,uint32_t bank) {
    const char *setting=getenv("DC_GAS_MULTIGRID");
    if(!t->practical||dt>.05f||!setting||strcmp(setting,"0")==0||(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)!=0u) return false;
    uint64_t cells=0;
    for(uint32_t level=1u;level<=3u;level++) {uint32_t scale=1u<<level;cells+=(uint64_t)((g->width+scale-1u)/scale)*((g->height+scale-1u)/scale);}
    return cells*32u+(uint64_t)g->width*g->height*4u<=t->transport_work.bytes;
}
#endif
