#include "ferrum/fire/bulk_rheology.h"

bool fr_bulk_rheology_mobility(const fr_reactive_profile_t *profile,
    float temperature_K,float stress,float shear_rate,float *out) {
    (void)profile;(void)temperature_K;(void)stress;(void)shear_rate;
    if(!out) return false;
    *out=1.f;
    return true;
}
