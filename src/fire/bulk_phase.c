#include <math.h>
#include <string.h>

#include "ferrum/fire/bulk_phase.h"

static uint64_t occupied_volume(const fr_bulk_cell_t *cell) {
    uint64_t occupied=0;
    for(uint32_t i=0;i<FR_BULK_PHASES;i++) occupied+=cell->volume_fp[i];
    return occupied;
}

static bool valid_cell(const fr_bulk_cell_t *cell) {
    if(!cell||cell->capacity_fp>FR_BULK_CELL_FULL_FP) return false;
    return occupied_volume(cell)<=cell->capacity_fp;
}

bool fr_bulk_phase_transfer(const fr_bulk_cell_t *source,
    const fr_bulk_cell_t *destination,uint32_t phase,uint32_t requested_fp,
    fr_bulk_cell_t *source_out,fr_bulk_cell_t *destination_out,
    uint32_t *moved_fp) {
    if(!valid_cell(source)||!valid_cell(destination)||phase>=FR_BULK_PHASES||
       !source_out||!destination_out||!moved_fp||source_out==destination_out) return false;
    uint64_t occupied=occupied_volume(destination);
    uint64_t stock=(uint64_t)source->volume_fp[phase]+source->retained_fp[phase];
    uint32_t admitted=requested_fp;
    if(stock<admitted) admitted=(uint32_t)stock;
    uint32_t free_fp=destination->capacity_fp-(uint32_t)occupied;
    if(free_fp<admitted) admitted=free_fp;
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

bool fr_bulk_phase_set_capacity(const fr_bulk_cell_t *source,uint32_t capacity_fp,
    const float density[FR_BULK_PHASES],fr_bulk_cell_t *out) {
    uint32_t order[FR_BULK_PHASES];
    if(!valid_cell(source)||capacity_fp>FR_BULK_CELL_FULL_FP||!out||
       !fr_bulk_phase_density_order(density,order)) return false;
    fr_bulk_cell_t next=*source;next.capacity_fp=capacity_fp;
    uint64_t occupied=occupied_volume(&next);
    if(occupied>capacity_fp) {
        uint32_t excess=(uint32_t)(occupied-capacity_fp);
        for(uint32_t i=0;i<FR_BULK_PHASES&&excess>0;i++) {
            uint32_t phase=order[i],moved=next.volume_fp[phase]<excess?
                next.volume_fp[phase]:excess;
            if(UINT32_MAX-next.retained_fp[phase]<moved) return false;
            next.volume_fp[phase]-=moved;next.retained_fp[phase]+=moved;excess-=moved;
        }
    } else {
        uint32_t free_fp=capacity_fp-(uint32_t)occupied;
        for(uint32_t i=FR_BULK_PHASES;i>0&&free_fp>0;i--) {
            uint32_t phase=order[i-1],moved=next.retained_fp[phase]<free_fp?
                next.retained_fp[phase]:free_fp;
            next.retained_fp[phase]-=moved;next.volume_fp[phase]+=moved;free_fp-=moved;
        }
    }
    *out=next;return true;
}
