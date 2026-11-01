#include <math.h>

#include "dungeoncraft/bulk_rheology_layout.h"
#include "ferrum/fire/bulk_phase.h"
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

bool fr_bulk_rheology_quadrature(uint32_t volume_fp,float density,
    uint32_t sample_count,fr_bulk_rheology_quadrature_t *out) {
    if(!out||volume_fp>FR_BULK_CELL_FULL_FP||!isfinite(density)||density<=0.f||
       sample_count==0u||sample_count>FR_BULK_RHEOLOGY_MAX_QUADRATURE) return false;
    uint32_t base=volume_fp/sample_count,remainder=volume_fp%sample_count;
    for(uint32_t i=0;i<sample_count;i++) {
        uint32_t weight=base+(i<remainder?1u:0u);
        out[i].volume_fp=weight;
        out[i].mass=density*(float)weight/(float)FR_BULK_CELL_FULL_FP;
    }
    return true;
}

bool fr_bulk_rheology_shear_rate(const float gradient[4],float *out) {
    if(!gradient||!out) return false;
    for(uint32_t i=0u;i<4u;i++) if(!isfinite(gradient[i])) return false;
    float shear=gradient[1]+gradient[2];
    if(!isfinite(shear)) return false;
    *out=fabsf(shear);
    return true;
}
