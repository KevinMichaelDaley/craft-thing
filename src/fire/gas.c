#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ferrum/fire/gas.h"
#include "gas_internal.h"

bool gas_error(char *err,uint32_t cap,const char *reason) {
    if(err&&cap) snprintf(err,cap,"%s",reason);
    return false;
}
bool fr_gas_thermodynamics(const fr_gas_model_t *m,const fr_gas_state_t *s,
    double volume,fr_gas_thermo_t *out,char *err,uint32_t cap) {
    if(!m||!s||!out||!isfinite(volume)||volume<0||!isfinite(m->kinetic_scale)||m->kinetic_scale<=0)
        return gas_error(err,cap,"Invalid gas model, volume or destination");
    double mass=0,capacity=0,moles=0;
    for(uint32_t i=0;i<FR_GAS_SPECIES;i++) {
        if(!isfinite(m->gas_constant[i])||m->gas_constant[i]<0||
           !isfinite(m->heat_capacity_cv[i])||m->heat_capacity_cv[i]<=0||
           !isfinite(s->species[i])||s->species[i]<0)
            return gas_error(err,cap,"Invalid gas species or thermodynamic coefficient");
        mass+=s->species[i];capacity+=s->species[i]*m->heat_capacity_cv[i];moles+=s->species[i]*m->gas_constant[i];
    }
    double tracer=s->species[FR_GAS_EXHAUST]+s->species[FR_GAS_SOOT];
    for(uint32_t i=0;i<3;i++)
        if(!isfinite(s->pigment[i])||s->pigment[i]<0||s->pigment[i]>tracer*(1.+16.*DBL_EPSILON))
            return gas_error(err,cap,"Gas pigment has no admitted carrier");
    if(!isfinite(s->energy)||s->energy<0||!isfinite(s->momentum[0])||!isfinite(s->momentum[1])||
       !isfinite(mass)||!isfinite(capacity)||!isfinite(moles)) return gas_error(err,cap,"Invalid gas energy or momentum");
    fr_gas_thermo_t value={.mass=mass};
    if(mass==0) {
        if(s->energy!=0||s->momentum[0]!=0||s->momentum[1]!=0)
            return gas_error(err,cap,"Empty gas retains energy or momentum");
    } else {
        if(volume<=0||capacity<=0) return gas_error(err,cap,"Gas inventory has no available volume");
        double kinetic=.5*m->kinetic_scale*(s->momentum[0]*s->momentum[0]+s->momentum[1]*s->momentum[1])/mass;
        value.temperature_K=(s->energy-kinetic)/capacity;
        value.pressure=moles*value.temperature_K/(volume*m->kinetic_scale);
        value.sound_speed=sqrt((1.+moles/capacity)*value.pressure*volume/mass);
        value.oxygen_mole_fraction=moles>0?s->species[FR_GAS_OXYGEN]*m->gas_constant[FR_GAS_OXYGEN]/moles:0;
        if(!isfinite(value.temperature_K)||value.temperature_K<=0||!isfinite(value.pressure)||
           !isfinite(value.sound_speed)) return gas_error(err,cap,"Gas has no positive finite internal energy");
    }
    *out=value;return true;
}
bool fr_gas_reference_compress(const fr_gas_model_t *m,const fr_gas_state_t *s,
    double old_volume,double new_volume,fr_gas_state_t *out,double *work,char *err,uint32_t cap) {
    fr_gas_thermo_t before,after;
    if(!out||!work||!isfinite(new_volume)||new_volume<=0||
       !fr_gas_thermodynamics(m,s,old_volume,&before,err,cap)) return gas_error(err,cap,"Invalid gas volume change");
    fr_gas_state_t value=*s;
    if(before.mass>0) {
        double capacity=0,moles=0;
        for(uint32_t i=0;i<FR_GAS_SPECIES;i++) {capacity+=s->species[i]*m->heat_capacity_cv[i];moles+=s->species[i]*m->gas_constant[i];}
        double internal=capacity*before.temperature_K;
        value.energy+=internal*(pow(old_volume/new_volume,moles/capacity)-1.);
    }
    if(!fr_gas_thermodynamics(m,&value,new_volume,&after,err,cap)) return false;
    *work=value.energy-s->energy;*out=value;return true;
}
bool fr_gas_admit(const fr_gas_model_t *m,const fr_gas_state_t *s,const fr_gas_state_t *t,
    double fraction,fr_gas_state_t *so,fr_gas_state_t *to,char *err,uint32_t cap) {
    fr_gas_thermo_t checked;
    if(!so||!to||so==to||s==t||!isfinite(fraction)||fraction<0||fraction>1||
       !fr_gas_thermodynamics(m,s,1,&checked,err,cap)||!fr_gas_thermodynamics(m,t,1,&checked,err,cap))
        return gas_error(err,cap,"Invalid finite gas admission");
    fr_gas_state_t left=gas_combine(s,1.-fraction,s,0),right=gas_combine(t,1.,s,fraction);
    if(!fr_gas_thermodynamics(m,&left,1,&checked,err,cap)||!fr_gas_thermodynamics(m,&right,1,&checked,err,cap)) return false;
    *so=left;*to=right;return true;
}
bool fr_gas_reference_buoyancy(const fr_gas_model_t *m,const fr_gas_state_t *s,
    double volume,double ambient_specific_RT,double gravity,double seconds,
    fr_gas_state_t *out,double *work,char *err,uint32_t cap) {
    (void)m;(void)s;(void)volume;(void)ambient_specific_RT;(void)gravity;(void)seconds;
    (void)out;(void)work;return gas_error(err,cap,"Buoyancy not implemented");
}
