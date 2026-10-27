#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/fire/material_profile.h"

#include "material_profile_data.inc"

_Static_assert(sizeof(fr_reactive_profile_t)==FR_PROFILE_WORDS*sizeof(uint32_t),"GPU profile stride changed");
_Static_assert(offsetof(fr_reactive_profile_t,fuel_yields)==56,"GPU profile yield offset changed");
_Static_assert(sizeof(profiles)/sizeof(profiles[0])==DC_MATERIAL_COUNT,"Material registry changed");

static bool error(char *err,uint32_t cap,const char *reason) {
    if(err&&cap) snprintf(err,cap,"%s",reason);
    return false;
}
uint32_t fr_reactive_profile_count(void) { return DC_MATERIAL_COUNT; }
const fr_reactive_profile_t *fr_reactive_profile(uint32_t material) {
    return material<fr_reactive_profile_count()?&profiles[material]:NULL;
}
const fr_reactive_profile_t *fr_reactive_profile_for_body(uint32_t material) {
    static const uint8_t terrain[]={0,DC_MATERIAL_STONE,DC_MATERIAL_WOOD,
        DC_MATERIAL_MORTAR_DEBRIS,DC_MATERIAL_DENSE_WOOD,DC_MATERIAL_LEAVES,
        DC_MATERIAL_SAND,DC_MATERIAL_DIRT,DC_MATERIAL_GRAVEL,DC_MATERIAL_LEAF_DEBRIS};
    return material>0&&material<sizeof(terrain)?fr_reactive_profile(terrain[material]):NULL;
}
const char *fr_reactive_material_name(uint32_t material) {
    return material<fr_reactive_profile_count()?profile_names[material]:NULL;
}
bool fr_reactive_profile_valid(const fr_reactive_profile_t *profile,
                               char *err, uint32_t cap) {
    if(!profile||profile->material>=DC_MATERIAL_COUNT||profile->phase>=FR_REACTIVE_PHASE_COUNT||
       (profile->flags&~FR_PROFILE_FLAG_MASK)!=0) return error(err,cap,"Invalid reactive material identity/flags");
    if(profile->legacy_body_material!=0) {
        const fr_reactive_profile_t *mapped=fr_reactive_profile_for_body(profile->legacy_body_material);
        if(!mapped||mapped->material!=profile->material)
            return error(err,cap,"Invalid legacy body-material mapping");
    }
    float values[FR_PROFILE_FLOAT_WORDS];memcpy(values,&profile->density,sizeof(values));
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++)
        if(!isfinite(values[i])||values[i]<0) return error(err,cap,"Invalid reactive coefficient");
    if(profile->density<=0||profile->heat_capacity<=0||profile->fuel_fraction>1||
       profile->fuel_fraction+profile->bound_oxidizer_fraction>1.000001f||
       profile->saturated_hazard_factor>1||profile->hot_yield_multiplier>1||
       profile->shear_exponent<=0) return error(err,cap,"Invalid physical composition/rheology");
    float yields=0;for(unsigned i=0;i<4;i++) yields+=profile->fuel_yields[i];
    if(fabsf(yields-1)>1e-5f||profile->primary_heat+
       profile->fuel_yields[0]*profiles[DC_MATERIAL_CHAR].chemical_energy>profile->chemical_energy+.01f)
        return error(err,cap,"Reaction products or retained-char energy are unbalanced");
    if(((profile->flags&FR_REACTIVE_FUEL)!=0)!=(profile->fuel_fraction>0)||
       ((profile->flags&FR_REACTIVE_BOUND_OXIDIZER)!=0)!=(profile->bound_oxidizer_fraction>0))
        return error(err,cap,"Fuel/oxidizer flags disagree with composition");
    return true;
}

static float clamp01(float value) {return fminf(1,fmaxf(0,value));}
static float profile_rate(const fr_reactive_profile_t *p,float temperature,float wetness,
                           float oxidation,const fr_reactive_exposure_t *e) {
    if(!p||(p->flags&FR_REACTIVE_FUEL)==0||temperature<p->ignition_K) return 0;
    float supply=(p->flags&FR_REACTIVE_BOUND_OXIDIZER)!=0?
        (p->oxygen_per_fuel>0?clamp01(e->bound_oxidizer_per_fuel/p->oxygen_per_fuel):1):
        e->oxygen_mass_fraction/ambient_oxygen_fraction;
    float dry=1-wetness;
    float wet=(p->flags&FR_REACTIVE_WATER_DOUSEABLE)!=0?
        dry*dry+wetness*p->saturated_hazard_factor:1;
    float oxide=1+oxidation*(p->oxidized_hazard_scale-1);
    return p->hazard_per_second*wet*oxide*supply;
}
bool fr_reactive_ignition_rates(const fr_reactive_profile_t *substrate,
                                const fr_reactive_profile_t *coating,
                                const fr_reactive_exposure_t *exposure,
                                fr_reactive_ignition_rates_t *out,
                                char *err, uint32_t cap) {
    if(!exposure||!out) return error(err,cap,"Missing ignition exposure/output");
    if(!fr_reactive_profile_valid(substrate,err,cap)||
       (coating&&!fr_reactive_profile_valid(coating,err,cap))) return false;
    const fr_reactive_exposure_t *e=exposure;
    float values[8];memcpy(values,e,sizeof(values));
    for(size_t i=0;i<8;i++)
        if(!isfinite(values[i])||values[i]<0) return error(err,cap,"Invalid ignition exposure");
    if(e->temperature_K<=0||e->saturation>1||e->coating_coverage>1||
       e->substrate_oxidation>1||e->coating_oxidation>1||e->oxygen_mass_fraction>1)
        return error(err,cap,"Ignition exposure exceeds its physical range");
    float capacity=substrate->heat_capacity+e->saturation*substrate->moisture_capacity*
        profiles[DC_MATERIAL_WATER].heat_capacity;
    float temperature=e->temperature_K+e->shock_energy_per_mass/capacity;
    if(!isfinite(temperature)) return error(err,cap,"Ignition temperature overflow");
    fr_reactive_ignition_rates_t result={
        profile_rate(substrate,temperature,e->saturation,e->substrate_oxidation,e),
        e->coating_coverage*profile_rate(coating,temperature,e->saturation,e->coating_oxidation,e)};
    if(coating&&(coating->flags&FR_REACTIVE_FUEL)!=0)
        result.substrate*=1+e->coating_coverage*fmaxf(0,coating->oil_hazard_factor-1);
    if(!isfinite(result.substrate)||!isfinite(result.coating))
        return error(err,cap,"Ignition hazard overflow");
    *out=result;return true;
}
