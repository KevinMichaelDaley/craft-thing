#include <math.h>

#include "dungeoncraft/bulk_rheology_layout.h"
#include "ferrum/fire/bulk_rheology.h"

bool fr_bulk_rheology_mobility(const fr_reactive_profile_t *profile,
    float temperature_K,float stress,float shear_rate,float *out) {
    if(!profile||!out||profile->phase!=FR_REACTIVE_LIQUID||
       !isfinite(temperature_K)||temperature_K<=0.f||
       !isfinite(stress)||stress<0.f||!isfinite(shear_rate)||shear_rate<0.f||
       !isfinite(profile->yield_stress)||profile->yield_stress<0.f||
       !isfinite(profile->consistency)||profile->consistency<0.f||
       !isfinite(profile->shear_exponent)||profile->shear_exponent<=0.f||
       !isfinite(profile->softening_K)||profile->softening_K<0.f||
       !isfinite(profile->hot_yield_multiplier)||profile->hot_yield_multiplier<0.f||
       profile->hot_yield_multiplier>1.f) return false;
    float hot=0.f;
    if(profile->softening_K>FR_BULK_RHEOLOGY_AMBIENT_K) {
        hot=(temperature_K-FR_BULK_RHEOLOGY_AMBIENT_K)/
            (profile->softening_K-FR_BULK_RHEOLOGY_AMBIENT_K);
        hot=fminf(1.f,fmaxf(0.f,hot));
    }
    float yield=profile->yield_stress*
        (1.f-hot*(1.f-profile->hot_yield_multiplier));
    float result=0.f;
    if(stress>yield) {
        float viscosity=profile->consistency*
            powf(fmaxf(shear_rate,FR_BULK_RHEOLOGY_SHEAR_FLOOR),
                profile->shear_exponent-1.f);
        result=(stress-yield)/(stress+viscosity);
    }
    *out=result;
    return true;
}
