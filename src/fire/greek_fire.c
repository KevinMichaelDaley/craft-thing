#include <math.h>
#include <stdio.h>

#include "dungeoncraft/coating_layout.h"
#include "ferrum/fire/greek_fire.h"
#include "ferrum/fire/material_profile.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}

static uint32_t credit(uint32_t packed,uint32_t shift) {
    return (packed>>shift)&FR_COATING_REACTION_REM_MASK;
}

static uint32_t replace_credit(uint32_t packed,uint32_t shift,uint32_t value) {
    uint32_t mask=FR_COATING_REACTION_REM_MASK<<shift;
    return (packed&~mask)|((value&FR_COATING_REACTION_REM_MASK)<<shift);
}

bool fr_coating_combustion_reference(const fr_reactive_state_t *source,
    uint32_t ambient_oxygen_fp,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_coating_combustion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    if(!source||!out||!products||!isfinite(exposed_fraction)||
       exposed_fraction<0.f||exposed_fraction>1.f||!isfinite(seconds)||seconds<0.f||
       !fr_reactive_state_valid(source,err_buf,err_cap))
        return error(err_buf,err_cap,"Invalid coating reaction input");
    fr_reactive_state_t next=*source;
    fr_coating_combustion_products_t result={0};
    uint64_t total=(uint64_t)source->coating_oil_fuel_fp+source->coating_jelly_fuel_fp;
    bool mixed=source->coating_oil_fuel_fp>0u&&source->coating_jelly_fuel_fp>0u;
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *jelly=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    const fr_reactive_profile_t *profile=fr_reactive_profile(mixed?
        DC_MATERIAL_GREEK_FIRE:DC_MATERIAL_OIL);
    if(!oil||!jelly||!profile||total>UINT32_MAX)
        return error(err_buf,err_cap,"Coating inventory exceeds finite storage");
    if(source->coating_oil_fuel_fp==0u) {
        next.flags&=~FR_REACTIVE_COATING_FLAMING;
        *out=next;*products=result;return true;
    }
    float temperature;
    if(!fr_reactive_state_temperature(source,&temperature,err_buf,err_cap)) return false;
    float wet=source->dry_mass_fp?
        fminf(1.f,(float)source->bound_water_fp/source->dry_mass_fp):0.f;
    bool viable=total>0u&&exposed_fraction>0.f&&
        temperature>=profile->ignition_K&&
        (mixed||wet<.8f)&&(!mixed||source->coating_oxidizer_fp>0u);
    if(!viable) next.flags&=~FR_REACTIVE_COATING_FLAMING;
    if(!viable||(next.flags&FR_REACTIVE_COATING_FLAMING)==0u) {
        *out=next;*products=result;return true;
    }
    double available=mixed?source->coating_oxidizer_fp:ambient_oxygen_fp;
    double oxygen_per_fuel=profile->oxygen_per_fuel;
    double fuel_credit=(double)credit(source->coating_remainders,
        FR_COATING_REACTION_REM_SHIFT)/1024.;
    double oxygen_credit=(double)credit(source->coating_remainders,
        FR_COATING_OXIDIZER_REM_SHIFT)/1024.;
    double rate=profile->surface_burn_per_second*seconds*exposed_fraction*
        (mixed?1.:(1.-wet)*(1.-wet));
    double requested=fmin((double)total-fuel_credit,
        (double)total*-expm1(-rate));
    double funded=fmin(requested,fmax(0.,(available+oxygen_credit)/oxygen_per_fuel-fuel_credit));
    if(!isfinite(funded)||funded<0.) return error(err_buf,err_cap,"Coating reaction rate overflow");
    double progressed=fuel_credit+funded;
    uint32_t reacted=(uint32_t)fmin((double)total,floor(progressed));
    double fractional=fmin(1023./1024.,fmax(0.,progressed-reacted));
    next.coating_remainders=replace_credit(next.coating_remainders,
        FR_COATING_REACTION_REM_SHIFT,(uint32_t)floor(fractional*1024.));
    if(reacted==0u) {
        *out=next;*products=result;return true;
    }
    double oxygen_needed=reacted*oxygen_per_fuel;
    uint32_t oxygen=(uint32_t)ceil(fmax(0.,oxygen_needed-oxygen_credit)-1e-9);
    if(oxygen>available) return error(err_buf,err_cap,"Coating oxygen reservation overflow");
    double remaining_credit=fmin(1023./1024.,
        fmax(0.,oxygen_credit+oxygen-oxygen_needed));
    next.coating_remainders=replace_credit(next.coating_remainders,
        FR_COATING_OXIDIZER_REM_SHIFT,(uint32_t)floor(remaining_credit*1024.));
    uint32_t oil_used=mixed?(uint32_t)((uint64_t)reacted*source->coating_oil_fuel_fp/total):reacted;
    uint32_t jelly_used=reacted-oil_used;
    const fr_reactive_profile_t *host=fr_reactive_profile(source->material);
    uint32_t ash=(uint32_t)floor(reacted*(double)profile->fuel_yields[1]);
    uint32_t soot=(uint32_t)floor(reacted*(double)profile->fuel_yields[2]);
    if(ash>reacted||soot>reacted-ash||next.inert_mass_fp>UINT32_MAX-ash)
        return error(err_buf,err_cap,"Coating product inventory overflow");
    next.coating_oil_fuel_fp-=oil_used;
    next.coating_jelly_fuel_fp-=jelly_used;
    if(mixed) next.coating_oxidizer_fp-=oxygen;
    next.inert_mass_fp+=ash;
    result=(fr_coating_combustion_products_t){.oil_fp=oil_used,.jelly_fp=jelly_used,
        .bound_oxidizer_fp=mixed?oxygen:0u,
        .ambient_oxygen_fp=mixed?0u:oxygen,.ash_fp=ash,.soot_fp=soot,
        .exhaust_fp=reacted+oxygen-ash-soot};
    double chemical=((double)oil_used*oil->chemical_energy+
        (double)jelly_used*jelly->chemical_energy)/65536.;
    result.released_heat=chemical;
    double removed=((double)oil_used*oil->heat_capacity+
        ((double)jelly_used+(mixed?oxygen:0u))*jelly->heat_capacity)*temperature/65536.;
    double retained=(double)ash*host->heat_capacity*temperature/65536.;
    result.exported_energy=.3*chemical+removed-retained;
    if(!fr_reactive_state_add_heat(&next,.7*chemical-removed+retained,
        err_buf,err_cap)) return false;
    if(next.coating_oil_fuel_fp==0u||
       (mixed&&(next.coating_jelly_fuel_fp==0u||next.coating_oxidizer_fp==0u)))
        next.flags&=~FR_REACTIVE_COATING_FLAMING;
    if(!fr_reactive_state_valid(&next,err_buf,err_cap)) return false;
    *out=next;*products=result;return true;
}
