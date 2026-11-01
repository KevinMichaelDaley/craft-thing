#include <math.h>
#include <string.h>

#include "ferrum/fire/bulk_phase.h"

static bool valid_cell(const fr_bulk_cell_t *cell) {
    if(!cell||cell->capacity_fp>65536u) return false;
    uint64_t occupied=0;
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) occupied+=cell->volume_fp[i];
    return occupied<=cell->capacity_fp;
}

bool fr_bulk_phase_transfer(const fr_bulk_cell_t *source,
    const fr_bulk_cell_t *destination,uint32_t phase,uint32_t requested_fp,
    fr_bulk_cell_t *source_out,fr_bulk_cell_t *destination_out,
    uint32_t *moved_fp) {
    if(!valid_cell(source)||!valid_cell(destination)||phase>=FR_BULK_PHASES||
       !source_out||!destination_out||!moved_fp||source_out==destination_out) return false;
    uint64_t occupied=0;
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) occupied+=destination->volume_fp[i];
    uint64_t stock=(uint64_t)source->volume_fp[phase]+source->retained_fp[phase];
    uint32_t admitted=(uint32_t)fmin((double)requested_fp,
        fmin((double)stock,(double)(destination->capacity_fp-occupied)));
    fr_bulk_cell_t next_source=*source,next_destination=*destination;
    uint32_t from_retained=admitted<next_source.retained_fp[phase]?
        admitted:next_source.retained_fp[phase];
    next_source.retained_fp[phase]-=from_retained;
    next_source.volume_fp[phase]-=admitted-from_retained;
    next_destination.volume_fp[phase]+=admitted;
    *source_out=next_source;*destination_out=next_destination;*moved_fp=admitted;
    return true;
}

bool fr_bulk_phase_density_order(const float density[FR_BULK_PHASES],
    uint32_t order[FR_BULK_PHASES]) {
    if(!density||!order) return false;
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) if(!isfinite(density[i])||density[i]<=0.) return false;
    uint32_t sorted[FR_BULK_PHASES];
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) sorted[i]=i;
    for(uint32_t i=1;i<FR_BULK_PHASES;i++) {
        uint32_t value=sorted[i],j=i;
        while(j>0&&density[sorted[j-1]]>density[value]) {sorted[j]=sorted[j-1];j--;}
        sorted[j]=value;
    }
    memcpy(order,sorted,sizeof(sorted));return true;
}
