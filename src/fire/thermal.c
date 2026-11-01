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
    fr_thermal_sample_t sample={(float)fr_reactive_state_energy(state),(float)fr_reactive_state_heat_capacity(state),
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
        double enthalpy=fr_reactive_state_energy(&b)*transfer/b.inert_mass_fp;
        if(!fr_reactive_state_add_heat(&a,enthalpy,err,cap)||!fr_reactive_state_add_heat(&b,-enthalpy,err,cap)) return false;
        b.inert_mass_fp-=transfer;b.dry_mass_fp=b.inert_mass_fp;
        a.bound_water_fp+=transfer;
    }
    if(!fr_reactive_state_valid(&a,err,cap)||!fr_reactive_state_valid(&b,err,cap)) return false;
    *solid_out=a;*water_out=b;return true;
}
bool fr_thermal_vapor_reference_energy(const fr_gas_model_t *model,double *out,char *err,uint32_t cap) {
    if(!model||!out||!isfinite(model->heat_capacity_cv[FR_GAS_VAPOR])||
       model->heat_capacity_cv[FR_GAS_VAPOR]<=0||
       !isfinite(model->gas_constant[FR_GAS_VAPOR])||model->gas_constant[FR_GAS_VAPOR]<0)
        return error(err,cap,"Invalid vapor energy model");
    const fr_reactive_profile_t *water=fr_reactive_profile(DC_MATERIAL_WATER);
    if(!water) return error(err,cap,"Missing liquid water profile");
    double value=FR_THERMAL_WATER_LATENT_KJ_PER_KG+
        (water->heat_capacity-model->heat_capacity_cv[FR_GAS_VAPOR]-
        model->gas_constant[FR_GAS_VAPOR])*FR_THERMAL_WATER_BOIL_K;
    if(!isfinite(value)||value<=0) return error(err,cap,"Invalid vapor reference energy");
    *out=value;return true;
}
bool fr_thermal_evaporate_pair(const fr_reactive_state_t *solid,const fr_gas_state_t *gas,
    const fr_gas_model_t *model,uint32_t requested_water_fp,fr_reactive_state_t *solid_out,
    fr_gas_state_t *gas_out,uint32_t *transferred_water_fp,char *err,uint32_t cap) {
    if(!solid_out||!gas_out||!transferred_water_fp||
       !fr_reactive_state_valid(solid,err,cap))
        return error(err,cap,"Invalid water-to-vapor transaction");
    const fr_reactive_profile_t *profile=fr_reactive_profile(solid->material);
    if(profile->phase!=FR_REACTIVE_SOLID)
        return error(err,cap,"Vapor source is not a solid surface");
    double offset=0;fr_gas_thermo_t gas_thermo;
    if(!fr_thermal_vapor_reference_energy(model,&offset,err,cap)||
       !fr_gas_thermodynamics(model,gas,1.,&gas_thermo,err,cap)) return false;
    fr_reactive_state_t next_solid=*solid;fr_gas_state_t next_gas=*gas;
    uint32_t moved=requested_water_fp<solid->bound_water_fp?
        requested_water_fp:solid->bound_water_fp;
    if(moved) {
        float temperature=0;
        if(!fr_reactive_state_temperature(solid,&temperature,err,cap)) return false;
        double per_kg=offset+model->heat_capacity_cv[FR_GAS_VAPOR]*temperature;
        double available=fr_reactive_state_energy(solid);
        if(!isfinite(per_kg)||per_kg<=0) return error(err,cap,"Invalid vapor heat budget");
        double capacity=floor(nextafter(available,0.)*DC_FLUID_FULL/per_kg);
        if(capacity<(double)moved) moved=capacity>0?(uint32_t)capacity:0u;
        if(moved) {
            double mass=(double)moved/DC_FLUID_FULL;
            double debit=mass*per_kg;
            if(!fr_reactive_state_add_heat(&next_solid,-debit,err,cap)) return false;
            next_solid.bound_water_fp-=moved;
            if(next_solid.bound_water_fp==0u) next_solid.evaporation_fraction=0.f;
            next_gas.species[FR_GAS_VAPOR]+=mass;
            next_gas.energy+=mass*model->heat_capacity_cv[FR_GAS_VAPOR]*temperature;
            if(!fr_reactive_state_valid(&next_solid,err,cap)||
               !fr_gas_thermodynamics(model,&next_gas,1.,&gas_thermo,err,cap)) return false;
        }
    }
    *solid_out=next_solid;*gas_out=next_gas;*transferred_water_fp=moved;
    return true;
}
bool fr_thermal_evaporation_step(const fr_reactive_state_t *solid,const fr_gas_state_t *gas,
    const fr_gas_model_t *model,double gas_volume,double ambient_density,double face_fraction,
    double seconds,fr_reactive_state_t *solid_out,fr_gas_state_t *gas_out,
    uint32_t *transferred_water_fp,char *err,uint32_t cap) {
    if(!solid_out||!gas_out||!transferred_water_fp||!isfinite(gas_volume)||gas_volume<=0||
       !isfinite(ambient_density)||ambient_density<=0||!isfinite(face_fraction)||
       face_fraction<0||face_fraction>1||!isfinite(seconds)||seconds<0||
       !fr_reactive_state_valid(solid,err,cap))
        return error(err,cap,"Invalid diffusive evaporation transaction");
    fr_gas_thermo_t gas_thermo;
    if(!fr_gas_thermodynamics(model,gas,gas_volume,&gas_thermo,err,cap)) return false;
    fr_reactive_state_t next_solid=*solid;fr_gas_state_t next_gas=*gas;
    uint32_t moved=0;
    if(seconds>0&&face_fraction>0&&solid->bound_water_fp>0) {
        double vapor_R=model->gas_constant[FR_GAS_VAPOR];
        if(vapor_R<=0) return error(err,cap,"Vapor needs a positive gas constant");
        float temperature=0;
        if(!fr_reactive_state_temperature(solid,&temperature,err,cap)) return false;
        double air_R=.21*model->gas_constant[FR_GAS_OXYGEN]+
            .79*model->gas_constant[FR_GAS_NITROGEN];
        double exponent=(FR_THERMAL_WATER_LATENT_KJ_PER_KG/vapor_R)*
            (1./FR_THERMAL_WATER_BOIL_K-1./temperature);
        double saturated=ambient_density*air_R*FR_THERMAL_AMBIENT_K*
            exp(exponent)*gas_volume/(vapor_R*temperature);
        double deficit=fmax(0.,saturated-gas->species[FR_GAS_VAPOR]);
        double rate=FR_THERMAL_VAPOR_DIFFUSIVITY_M2_PER_S/
            (FR_THERMAL_GAMEPLAY_FACE_METERS*FR_THERMAL_GAMEPLAY_FACE_METERS);
        double requested=fmin(deficit*DC_FLUID_FULL,
            solid->evaporation_fraction+deficit*DC_FLUID_FULL*rate*face_fraction*seconds);
        if(!isfinite(requested)||requested<0)
            return error(err,cap,"Diffusive evaporation flux exceeds storage");
        uint32_t whole=requested>=UINT32_MAX?UINT32_MAX:(uint32_t)floor(requested);
        double fraction=requested-whole;
        if(whole) {
            if(!fr_thermal_evaporate_pair(solid,gas,model,whole,&next_solid,&next_gas,&moved,err,cap)) return false;
        }
        if(next_solid.bound_water_fp>0) next_solid.evaporation_fraction=(float)fraction;
        if(!fr_reactive_state_valid(&next_solid,err,cap)) return false;
    }
    *solid_out=next_solid;*gas_out=next_gas;*transferred_water_fp=moved;
    return true;
}
