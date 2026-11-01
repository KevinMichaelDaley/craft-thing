#include <math.h>
#include <stdio.h>

#include "dungeoncraft/corrosion_parameters.h"
#include "ferrum/fire/corrosion.h"
#include "ferrum/fire/material_profile.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}

static uint32_t ceil_div(uint32_t numerator,uint32_t denominator) {
    return numerator/denominator+(numerator%denominator!=0u);
}

static float reagent_rate(const fr_reactive_state_t *source,fr_corrosion_reagent_t reagent,
    float oxygen_fraction,float saturation,float temperature) {
    if(reagent==FR_CORROSION_AIR) {
        if(source->material!=DC_MATERIAL_STONE||temperature<273.15f) return 0.f;
        return FR_CORROSION_STONE_AIR_RATE*saturation*
            oxygen_fraction/FR_CORROSION_AMBIENT_OXYGEN_FRACTION;
    }
    if(temperature<FR_CORROSION_GREEK_MIN_K||source->coating_jelly_fuel_fp==0u||
       source->coating_oxidizer_fp==0u||source->dry_mass_fp==0u) return 0.f;
    float coverage=fminf(1.f,(float)source->coating_jelly_fuel_fp/source->dry_mass_fp);
    if(source->material==DC_MATERIAL_GOLD) return FR_CORROSION_GOLD_GREEK_RATE*coverage;
    if(source->material==DC_MATERIAL_MAGIC_INGREDIENT)
        return FR_CORROSION_INGREDIENT_GREEK_RATE*coverage;
    return 0.f;
}

bool fr_corrosion_reference(const fr_reactive_state_t *source,
    fr_corrosion_reagent_t reagent,uint32_t available_oxygen_fp,
    float oxygen_fraction,float saturation,float seconds,
    fr_reactive_state_t *out,fr_corrosion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    if(!source||!out||!products||
       (reagent!=FR_CORROSION_AIR&&reagent!=FR_CORROSION_GREEK_FIRE)||
       !isfinite(oxygen_fraction)||oxygen_fraction<0.f||oxygen_fraction>1.f||
       !isfinite(saturation)||saturation<0.f||saturation>1.f||
       !isfinite(seconds)||seconds<0.f||
       !fr_reactive_state_valid(source,err_buf,err_cap))
        return error(err_buf,err_cap,"Invalid corrosion transaction");
    fr_reactive_state_t next=*source;
    fr_corrosion_products_t result={0};
    float progress=reagent==FR_CORROSION_AIR?source->oxidized_fraction:source->corroded_fraction;
    if((source->flags&FR_REACTIVE_SPENT)!=0u||source->dry_mass_fp==0u) {
        result.section_strength_fraction=fmaxf(FR_CORROSION_SPENT_STRENGTH,
            1.f-(1.f-FR_CORROSION_SPENT_STRENGTH)*progress);
        *out=next;*products=result;return true;
    }
    float temperature;
    if(!fr_reactive_state_temperature(source,&temperature,err_buf,err_cap)) return false;
    float rate=reagent_rate(source,reagent,oxygen_fraction,saturation,temperature);
    if(!isfinite(rate)) return error(err_buf,err_cap,"Corrosion rate is not finite");
    float target=fminf(1.f,progress+rate*seconds);
    uint32_t old_units=(uint32_t)floor((double)source->dry_mass_fp*progress);
    uint32_t target_units=(uint32_t)floor((double)source->dry_mass_fp*target);
    if(target_units<old_units) target_units=old_units;
    uint32_t denominator=reagent==FR_CORROSION_AIR?
        FR_CORROSION_AIR_OXIDIZER_DENOMINATOR:FR_CORROSION_GREEK_OXIDIZER_DENOMINATOR;
    uint32_t available=reagent==FR_CORROSION_AIR?available_oxygen_fp:source->coating_oxidizer_fp;
    uint64_t maximum=((uint64_t)ceil_div(old_units,denominator)+available)*denominator;
    if(target_units>maximum) {
        target_units=(uint32_t)fmin((double)source->dry_mass_fp,(double)maximum);
        target=(float)target_units/source->dry_mass_fp;
    }
    uint32_t converted=target_units-old_units;
    uint32_t oxidizer=ceil_div(target_units,denominator)-ceil_div(old_units,denominator);
    if(oxidizer>available||next.core_oxidizer_fp>UINT32_MAX-oxidizer||
       converted>UINT32_MAX-oxidizer)
        return error(err_buf,err_cap,"Corrosion inventory exceeds finite storage");
    next.core_oxidizer_fp+=oxidizer;
    if(reagent==FR_CORROSION_AIR) next.oxidized_fraction=target;
    else {
        next.coating_oxidizer_fp-=oxidizer;
        next.corroded_fraction=target;
        if(source->material==DC_MATERIAL_MAGIC_INGREDIENT) {
            const fr_reactive_profile_t *profile=fr_reactive_profile(source->material);
            uint32_t initial=(uint32_t)llround(source->dry_mass_fp*(double)profile->fuel_fraction);
            uint32_t desired=(uint32_t)floor(initial*(1.-(double)target));
            uint32_t moved=next.fuel_mass_fp>desired?next.fuel_mass_fp-desired:0u;
            if(next.inert_mass_fp>UINT32_MAX-moved)
                return error(err_buf,err_cap,"Spent ingredient mass exceeds finite storage");
            next.fuel_mass_fp-=moved;next.inert_mass_fp+=moved;
        }
        if(target>=1.f) {
            if(source->material==DC_MATERIAL_GOLD) next.material=DC_MATERIAL_CORROSION_RESIDUE;
            if(source->material==DC_MATERIAL_MAGIC_INGREDIENT) next.material=DC_MATERIAL_ASH;
            next.flags|=FR_REACTIVE_SPENT;
        }
    }
    result.converted_fp=converted;
    result.oxidizer_fp=oxidizer;
    result.residue_fp=converted+oxidizer;
    result.section_strength_fraction=fmaxf(FR_CORROSION_SPENT_STRENGTH,
        1.f-(1.f-FR_CORROSION_SPENT_STRENGTH)*target);
    if(!fr_reactive_state_valid(&next,err_buf,err_cap)) return false;
    *out=next;*products=result;return true;
}

uint32_t fr_corrosion_pristine_mass_fp(const fr_reactive_state_t *state) {
    if(!state||!fr_reactive_state_valid(state,NULL,0u)||
       state->material==DC_MATERIAL_CORROSION_RESIDUE||
       (state->material==DC_MATERIAL_ASH&&(state->flags&FR_REACTIVE_SPENT)!=0u)) return 0u;
    float progress=fmaxf(state->oxidized_fraction,state->corroded_fraction);
    return (uint32_t)floor((double)state->dry_mass_fp*(1.-progress));
}
