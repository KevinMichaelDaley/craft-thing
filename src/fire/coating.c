#include <math.h>
#include <string.h>

#include "ferrum/fire/coating.h"
#include "ferrum/fire/material_profile.h"

#define FR_COATING_SOFTENING_SPAN_K 100.f

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

static float adhesion_at_temperature(const fr_reactive_profile_t *profile,
    float temperature_K) {
    float blend=fminf(1.f,fmaxf(0.f,
        (temperature_K-profile->softening_K)/FR_COATING_SOFTENING_SPAN_K));
    return profile->cold_adhesion+blend*
        (profile->hot_adhesion-profile->cold_adhesion);
}

static double film_capacity(const fr_coating_film_t *film,
    const fr_reactive_profile_t *oil,const fr_reactive_profile_t *slime) {
    return (double)film->oil_fp*oil->density*oil->heat_capacity+
        (double)film->slime_fp*slime->density*slime->heat_capacity;
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

bool fr_coating_release(const fr_coating_film_t *film,
    const fr_bulk_cell_t *receiver,double receiver_excess_heat,
    uint32_t phase,float temperature_K,float traction,float seconds,
    fr_coating_film_t *film_out,fr_bulk_cell_t *receiver_out,
    double *receiver_excess_out,uint32_t *released_fp) {
    if(!film||!valid_host(&film->host)||!valid_bulk(receiver)||
       (phase!=FR_BULK_OIL&&phase!=FR_BULK_SLIME)||
       !isfinite(receiver_excess_heat)||!isfinite(film->excess_heat)||
       !isfinite(temperature_K)||temperature_K<=0.f||
       !isfinite(traction)||traction<0.f||!isfinite(seconds)||seconds<0.f||
       !film_out||!receiver_out||!receiver_excess_out||!released_fp)
        return false;
    uint32_t stock=phase==FR_BULK_OIL?film->oil_fp:film->slime_fp;
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *slime=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    const fr_reactive_profile_t *profile=phase==FR_BULK_OIL?oil:slime;
    if(!oil||!slime||profile->consistency<=0.f||profile->shear_exponent<=0.f||
       (film->oil_fp==0u&&film->slime_fp==0u&&film->excess_heat!=0.))
        return false;
    float adhesion=adhesion_at_temperature(profile,temperature_K);
    double drive=fmax(0.,(double)traction-adhesion)/profile->consistency;
    double mobile=stock*fmin(1.,pow(drive,1./profile->shear_exponent)*seconds);
    uint32_t requested=(uint32_t)fmin((double)stock,floor(mobile));
    uint64_t occupied=0u;
    for(uint32_t i=0u;i<FR_BULK_PHASES;i++) occupied+=receiver->volume_fp[i];
    uint32_t room=receiver->capacity_fp-(uint32_t)occupied;
    uint32_t released=requested<room?requested:room;
    double transfer_capacity=(double)released*profile->density*profile->heat_capacity;
    double total_capacity=film_capacity(film,oil,slime);
    double heat=total_capacity>0.?film->excess_heat*transfer_capacity/total_capacity:0.;
    double next_film_heat=film->excess_heat-heat;
    double next_receiver_heat=receiver_excess_heat+heat;
    if(!isfinite(mobile)||!isfinite(heat)||!isfinite(next_film_heat)||
       !isfinite(next_receiver_heat)) return false;
    fr_coating_film_t next_film=*film;
    fr_bulk_cell_t next_receiver=*receiver;
    if(phase==FR_BULK_OIL) next_film.oil_fp-=released;
    else next_film.slime_fp-=released;
    next_film.excess_heat=next_film_heat;
    next_receiver.volume_fp[phase]+=released;
    *film_out=next_film;*receiver_out=next_receiver;
    *receiver_excess_out=next_receiver_heat;*released_fp=released;
    return true;
}

bool fr_coating_film_split(const fr_coating_film_t *source,
    uint32_t left_weight_fp,const fr_coating_host_t *left_host,
    const fr_coating_host_t *right_host,fr_coating_film_t *left,
    fr_coating_film_t *right) {
    if(!source||!valid_host(&source->host)||
       left_weight_fp==0u||left_weight_fp>=FR_BULK_CELL_FULL_FP||
       !valid_host(left_host)||!valid_host(right_host)||
       memcmp(left_host,right_host,sizeof(*left_host))==0||
       !left||!right||left==right||!isfinite(source->excess_heat)||
       (source->oil_fp==0u&&source->slime_fp==0u&&source->excess_heat!=0.))
        return false;
    fr_coating_film_t a=*source,b=*source;
    a.host=*left_host;b.host=*right_host;
    a.oil_fp=(uint32_t)((uint64_t)source->oil_fp*left_weight_fp/
        FR_BULK_CELL_FULL_FP);
    a.slime_fp=(uint32_t)((uint64_t)source->slime_fp*left_weight_fp/
        FR_BULK_CELL_FULL_FP);
    b.oil_fp=source->oil_fp-a.oil_fp;
    b.slime_fp=source->slime_fp-a.slime_fp;
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *slime=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    if(!oil||!slime) return false;
    double source_capacity=film_capacity(source,oil,slime);
    double left_capacity=film_capacity(&a,oil,slime);
    a.excess_heat=source_capacity>0.?
        source->excess_heat*left_capacity/source_capacity:0.;
    b.excess_heat=source->excess_heat-a.excess_heat;
    if(!isfinite(a.excess_heat)||!isfinite(b.excess_heat)) return false;
    *left=a;*right=b;
    return true;
}
