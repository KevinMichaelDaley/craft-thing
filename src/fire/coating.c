#include <math.h>

#include "ferrum/fire/coating.h"

static bool valid_host(const fr_coating_host_t *host) {
    if(!host||host->kind<FR_COATING_TERRAIN||host->kind>FR_COATING_ACTOR)
        return false;
    if(host->kind==FR_COATING_TERRAIN) return host->rest_x<DC_CHUNK_SIDE&&
        host->rest_y<DC_CHUNK_SIDE;
    return (host->id_low||host->id_high)&&host->generation!=0u;
}

static bool valid_bulk(const fr_bulk_cell_t *bulk) {
    if(!bulk||bulk->capacity_fp>FR_BULK_CELL_FULL_FP) return false;
    uint64_t occupied=0u;
    for(uint32_t i=0u;i<FR_BULK_PHASES;i++) occupied+=bulk->volume_fp[i];
    return occupied<=bulk->capacity_fp;
}

bool fr_coating_contact_wet(const fr_bulk_cell_t *bulk,
    double bulk_excess_heat,const fr_coating_film_t *film,
    uint32_t phase,uint32_t requested_fp,fr_bulk_cell_t *bulk_out,
    double *bulk_excess_out,fr_coating_film_t *film_out,
    uint32_t *moved_fp) {
    if(!valid_bulk(bulk)||!film||!valid_host(&film->host)||
       (phase!=FR_BULK_OIL&&phase!=FR_BULK_SLIME)||
       !isfinite(bulk_excess_heat)||!isfinite(film->excess_heat)||
       !bulk_out||!bulk_excess_out||!film_out||!moved_fp||
       (film->oil_fp==0u&&film->slime_fp==0u&&film->excess_heat!=0.))
        return false;
    uint32_t existing=phase==FR_BULK_OIL?film->oil_fp:film->slime_fp;
    uint32_t available=bulk->volume_fp[phase];
    if(available==0u&&bulk_excess_heat!=0.) return false;
    uint32_t moved=requested_fp<available?requested_fp:available;
    if(moved>UINT32_MAX-existing) moved=UINT32_MAX-existing;
    double heat=moved&&available?bulk_excess_heat*(double)moved/available:0.;
    double next_bulk_heat=bulk_excess_heat-heat;
    double next_film_heat=film->excess_heat+heat;
    if(!isfinite(heat)||!isfinite(next_bulk_heat)||!isfinite(next_film_heat))
        return false;
    fr_bulk_cell_t next_bulk=*bulk;
    fr_coating_film_t next_film=*film;
    next_bulk.volume_fp[phase]-=moved;
    if(phase==FR_BULK_OIL) next_film.oil_fp+=moved;
    else next_film.slime_fp+=moved;
    next_film.excess_heat=next_film_heat;
    *bulk_out=next_bulk;
    *bulk_excess_out=next_bulk_heat;
    *film_out=next_film;
    *moved_fp=moved;
    return true;
}
