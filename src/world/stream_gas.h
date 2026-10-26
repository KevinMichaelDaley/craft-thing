#ifndef DC_STREAM_GAS_H
#define DC_STREAM_GAS_H
#include <math.h>
#include "ferrum/fire/gas_checkpoint.h"
static bool gas_checkpoint_valid(const fr_gas_checkpoint_chunk_t *gas) {
    if(!gas) return true;
    if(!isfinite(gas->clock.advanced_seconds)||gas->clock.advanced_seconds<0.||
       gas->clock.substeps<gas->clock.physical_steps||
       (gas->clock.physical_steps==0u&&gas->clock.advanced_seconds!=0.)) return false;
    for(uint32_t i=0;i<4096u;i++) {
        const float *p=gas->cells[i];
        for(uint32_t k=0;k<16u;k++) if(!isfinite(p[k])) return false;
        for(uint32_t k=0;k<5u;k++) if(p[k]<0.f) return false;
        for(uint32_t k=8u;k<11u;k++) if(p[k]<0.f||p[k]>(p[2]+p[4])*1.000001f) return false;
        if(p[12]<0.f||p[12]>1.f||p[13]<0.f||p[13]>1.f||(gas->gates[i]&~3u)!=0u) return false;
        double mass=(double)p[0]+p[1]+p[2]+p[3]+p[4],energy=(double)p[5]+p[11]+p[14];
        if(mass==0.) {if(energy!=0.||p[6]!=0.f||p[7]!=0.f) return false;}
        else if(p[12]<=0.f||energy<=0.) return false;
    }
    return true;
}
#endif
