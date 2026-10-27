#include <math.h>
#include <stdio.h>
#include "dungeoncraft/combustion_parameters.h"
#include "ferrum/fire/combustion.h"
bool fr_combustion_gas_reference(const fr_gas_model_t *model,const fr_gas_state_t *source,
    double volume,double seconds,fr_gas_state_t *out,double *reacted,char *err,uint32_t cap) {
    if(!out||!reacted||!isfinite(seconds)||seconds<0) {
        if(err&&cap) snprintf(err,cap,"Invalid airborne combustion interval");
        return false;
    }
    fr_gas_thermo_t thermo;
    if(!fr_gas_thermodynamics(model,source,volume,&thermo,err,cap)) return false;
    fr_gas_state_t value=*source;double extent=0;
    double oxygen=thermo.mass>0?source->species[FR_GAS_OXYGEN]/thermo.mass:0;
    double thermal=fmin(1,fmax(0,(thermo.temperature_K-FR_COMBUSTION_AIRBORNE_IGNITION_K)/
        (FR_COMBUSTION_AIRBORNE_FULL_RATE_K-FR_COMBUSTION_AIRBORNE_IGNITION_K)));
    double supply=fmin(1,fmax(0,(oxygen-FR_COMBUSTION_EXTINGUISH_OXYGEN)/
        (FR_COMBUSTION_AMBIENT_OXYGEN-FR_COMBUSTION_EXTINGUISH_OXYGEN)));
    if(source->species[FR_GAS_SOOT]>0&&thermal>0&&supply>0&&seconds>0) {
        extent=source->species[FR_GAS_SOOT]*-expm1(-FR_COMBUSTION_AIRBORNE_RATE*thermal*supply*seconds);
        extent=fmin(extent,source->species[FR_GAS_OXYGEN]/FR_COMBUSTION_AIRBORNE_OXYGEN);
        double used=extent*FR_COMBUSTION_AIRBORNE_OXYGEN;
        value.species[FR_GAS_SOOT]-=extent;value.species[FR_GAS_OXYGEN]-=used;
        value.species[FR_GAS_EXHAUST]+=extent+used;value.energy+=extent*FR_COMBUSTION_AIRBORNE_ENERGY;
    }
    if(!fr_gas_thermodynamics(model,&value,volume,&thermo,err,cap)) return false;
    *out=value;*reacted=extent;return true;
}
