#include <float.h>
#include <math.h>
#include <stdio.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static bool sample_valid(fr_thermal_sample_t sample) {
    return isfinite(sample.energy)&&isfinite(sample.capacity)&&isfinite(sample.conductivity)&&
        sample.energy>=0&&sample.capacity>0&&sample.conductivity>=0;
}
bool fr_thermal_sample_from_state(const fr_reactive_state_t *state,
    fr_thermal_sample_t *out,char *err,uint32_t cap) {
    if(!out||!fr_reactive_state_valid(state,err,cap)) return false;
    fr_thermal_sample_t sample={state->thermal_energy,(float)fr_reactive_state_heat_capacity(state),
        fr_reactive_profile(state->material)->conductivity};
    if(!sample_valid(sample)) return error(err,cap,"Thermal sample has no finite heat capacity");
    *out=sample;return true;
}
bool fr_thermal_pair_flux(fr_thermal_sample_t first,fr_thermal_sample_t second,
    float seconds,float *out,char *err,uint32_t cap) {
    if(!out||!sample_valid(first)||!sample_valid(second)||!isfinite(seconds)||seconds<0)
        return error(err,cap,"Invalid thermal face exchange");
    double a=(double)first.energy/first.capacity,b=(double)second.energy/second.capacity;
    double sum=(double)first.conductivity+second.conductivity;
    double conductance=sum>0?2.0*first.conductivity*second.conductivity/sum:0;
    double difference=a-b,requested=conductance*difference*seconds;
    double bound=fmin(first.capacity,second.capacity)*fabs(difference)/FR_THERMAL_NEIGHBORS;
    double flux=copysign(fmin(fabs(requested),bound),requested);
    if((flux>0&&flux>FLT_MAX-(double)second.energy)||(flux<0&&-flux>FLT_MAX-(double)first.energy))
        return error(err,cap,"Thermal face energy exceeds storage range");
    *out=(float)flux;return true;
}
bool fr_thermal_probe_exchange(fr_thermal_probe_t probe,fr_thermal_sample_t target,
    float seconds,fr_thermal_probe_t *probe_out,fr_thermal_sample_t *target_out,char *err,uint32_t cap) {
    if(!probe_out||!target_out||!isfinite(probe.capacity)||probe.capacity<=0||
       !isfinite(probe.excess_energy)||probe.excess_energy<0)
        return error(err,cap,"Invalid finite thermal probe");
    fr_thermal_sample_t source={.energy=(float)(probe.capacity*FR_THERMAL_AMBIENT_K+probe.excess_energy),
        .capacity=probe.capacity,.conductivity=FR_THERMAL_PROBE_CONDUCTIVITY};
    float flux=0;
    if(!fr_thermal_pair_flux(source,target,seconds,&flux,err,cap)) return false;
    if(flux>probe.excess_energy) flux=probe.excess_energy;
    probe.excess_energy-=flux;target.energy+=flux;
    if(!sample_valid(target)||!isfinite(probe.excess_energy)||probe.excess_energy<0)
        return error(err,cap,"Thermal probe output exceeds energy range");
    *probe_out=probe;*target_out=target;return true;
}
bool fr_thermal_absorb_pair(const fr_reactive_state_t *solid,const fr_reactive_state_t *water,
    float seconds,fr_reactive_state_t *solid_out,fr_reactive_state_t *water_out,char *err,uint32_t cap) {
    if(!solid_out||!water_out||solid_out==water_out||!isfinite(seconds)||seconds<0||
       !fr_reactive_state_valid(solid,err,cap)||!fr_reactive_state_valid(water,err,cap))
        return error(err,cap,"Invalid thermal wetting transaction");
    const fr_reactive_profile_t *profile=fr_reactive_profile(solid->material);
    if(profile->phase!=FR_REACTIVE_SOLID||water->material!=DC_MATERIAL_WATER||
       water->fuel_mass_fp||water->char_mass_fp||water->bound_water_fp||water->coating_oil_fuel_fp||
       water->coating_jelly_fuel_fp||water->core_oxidizer_fp||water->coating_oxidizer_fp)
        return error(err,cap,"Thermal wetting needs a solid and pure liquid water");
    fr_reactive_state_t a=*solid,b=*water;
    double capacity=round((double)profile->moisture_capacity*solid->dry_mass_fp);
    uint32_t maximum=capacity>UINT32_MAX?UINT32_MAX:(uint32_t)capacity;
    uint32_t room=maximum>a.bound_water_fp?(maximum-a.bound_water_fp)/FR_THERMAL_NEIGHBORS:0;
    double budget=seconds*FR_THERMAL_WETTING_PER_SECOND*DC_FLUID_FULL/FR_THERMAL_NEIGHBORS;
    uint32_t transfer=budget>UINT32_MAX?UINT32_MAX:(uint32_t)floor(budget);
    if(transfer>room) transfer=room;
    if(transfer>b.inert_mass_fp/FR_THERMAL_NEIGHBORS) transfer=b.inert_mass_fp/FR_THERMAL_NEIGHBORS;
    if(transfer) {
        float enthalpy=(float)((double)b.thermal_energy*transfer/b.inert_mass_fp);
        b.inert_mass_fp-=transfer;b.dry_mass_fp=b.inert_mass_fp;b.thermal_energy-=enthalpy;
        a.bound_water_fp+=transfer;a.thermal_energy+=enthalpy;
    }
    if(!fr_reactive_state_valid(&a,err,cap)||!fr_reactive_state_valid(&b,err,cap)) return false;
    *solid_out=a;*water_out=b;return true;
}
