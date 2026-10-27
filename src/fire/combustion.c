#include <math.h>
#include <stdio.h>
#include "dungeoncraft/combustion_parameters.h"
#include "ferrum/fire/combustion.h"
#include "ferrum/fire/material_profile.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static float threshold(const fr_reactive_state_t *s) {
    uint32_t h=s->origin_id_low^(s->origin_id_high*0x9e3779b9u);
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
    return (float)-log(((double)(h>>8)+.5)/16777216.0);
}
static void solid_products(uint32_t consumed,const fr_reactive_profile_t *p,uint32_t out[3]) {
    out[0]=(uint32_t)floor(consumed*(double)p->fuel_yields[0]);
    uint32_t rest=consumed-out[0];
    out[1]=(uint32_t)floor(rest*(double)p->fuel_yields[1]/(1-p->fuel_yields[0]));
    rest-=out[1];
    out[2]=(uint32_t)floor(rest*(double)p->fuel_yields[2]/(1-p->fuel_yields[0]-p->fuel_yields[1]));
}
bool fr_combustion_reference(const fr_reactive_state_t *source,double oxygen_fp,
    float oxygen_fraction,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_combustion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    if(!out||!products||!isfinite(oxygen_fp)||oxygen_fp<0||!isfinite(seconds)||seconds<0||!isfinite(exposed_fraction)||
       exposed_fraction<0||exposed_fraction>1||!isfinite(oxygen_fraction)||
       oxygen_fraction<0||oxygen_fraction>1||
       !fr_reactive_state_valid(source,err_buf,err_cap))
        return error(err_buf,err_cap,"Invalid ordinary combustion input");
    fr_reactive_state_t s=*source;fr_combustion_products_t p={0};
    bool char_burn=s.fuel_mass_fp==0&&s.char_mass_fp>0;
    uint32_t fuel=char_burn?s.char_mass_fp:s.fuel_mass_fp;
    const fr_reactive_profile_t *profile=fr_reactive_profile(char_burn?DC_MATERIAL_CHAR:s.material);
    if(profile->bound_oxidizer_fraction||s.coating_oil_fuel_fp||s.coating_jelly_fuel_fp)
        return error(err_buf,err_cap,"Ordinary substrate reference does not admit coatings or bound oxidizer");
    float temperature;
    if(!fr_reactive_state_temperature(&s,&temperature,err_buf,err_cap)) return false;
    float saturation=profile->moisture_capacity>0&&s.dry_mass_fp?
        fminf(1,(float)s.bound_water_fp/(s.dry_mass_fp*profile->moisture_capacity)):0;
    fr_reactive_exposure_t exposure={.temperature_K=temperature,.saturation=saturation,
        .substrate_oxidation=s.oxidized_fraction,.oxygen_mass_fraction=oxygen_fraction};
    fr_reactive_ignition_rates_t rates;
    if(!fr_reactive_ignition_rates(profile,NULL,&exposure,&rates,err_buf,err_cap)) return false;
    if(s.ignition_threshold==0) s.ignition_threshold=threshold(&s);
    s.ignition_integral=fminf(1e20f,s.ignition_integral+rates.substrate*exposed_fraction*seconds);
    bool viable=fuel&&oxygen_fp&&oxygen_fraction>.12f&&
        temperature>=profile->ignition_K&&saturation<.8f&&exposed_fraction>0;
    if(!viable) s.flags&=~FR_REACTIVE_FLAMING;
    else if(s.ignition_integral>=s.ignition_threshold) s.flags|=FR_REACTIVE_FLAMING;
    if(viable&&(s.flags&FR_REACTIVE_FLAMING)) {
        double wet=(1-saturation)*(1-saturation);
        double requested=(double)s.dry_mass_fp*profile->surface_burn_per_second*
            seconds*exposed_fraction*wet+s.reaction_fraction;
        s.reaction_fraction=(float)(requested-floor(requested));
        if(s.reaction_fraction>=1) s.reaction_fraction=nextafterf(1,0);
        double admitted=fmin(fuel,requested);
        if(profile->oxygen_per_fuel>0) admitted=fmin(admitted,oxygen_fp/(double)profile->oxygen_per_fuel);
        p.fuel_fp=(uint32_t)floor(admitted);
        p.oxygen_fp=p.fuel_fp*(double)profile->oxygen_per_fuel;
        uint32_t initial=(uint32_t)llround(s.dry_mass_fp*(double)profile->fuel_fraction);
        uint32_t consumed=initial>=fuel?initial-fuel:0;
        uint32_t before[3],after[3];
        solid_products(consumed,profile,before);solid_products(consumed+p.fuel_fp,profile,after);
        uint32_t char_fp=after[0]-before[0];
        p.ash_fp=after[1]-before[1];p.soot_fp=after[2]-before[2];
        p.exhaust_fp=p.fuel_fp-char_fp-p.ash_fp-p.soot_fp+p.oxygen_fp;
        p.reserved_airborne_heat=p.soot_fp/65536.0*FR_COMBUSTION_AIRBORNE_ENERGY;
        p.released_heat=p.fuel_fp/65536.0*profile->primary_heat-p.reserved_airborne_heat;
        double sensible=p.fuel_fp/65536.0*profile->heat_capacity*temperature;
        double retained=char_fp/65536.0*fr_reactive_profile(DC_MATERIAL_CHAR)->heat_capacity*temperature;
        p.exported_energy=sensible-retained+.3*p.released_heat;
        if(char_burn) s.char_mass_fp-=p.fuel_fp;else s.fuel_mass_fp-=p.fuel_fp;
        s.char_mass_fp+=char_fp;
        if(!fr_reactive_state_add_heat(&s,.7*p.released_heat-sensible+retained,err_buf,err_cap)) return false;
        if(!s.fuel_mass_fp&&!s.char_mass_fp) s.flags&=~FR_REACTIVE_FLAMING;
    }
    if(!fr_reactive_state_valid(&s,err_buf,err_cap)) return false;
    *out=s;*products=p;return true;
}
