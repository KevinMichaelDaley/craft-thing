#ifndef DC_CHAR_ENTHALPY_H
#define DC_CHAR_ENTHALPY_H

/* Effective char heat capacity rises above 1800 K to 2.08 J/(g K)
 * at 2500 K. Energy is the integral of that capacity, never a temperature cap. */
#define FR_CHAR_BASE_HEAT_CAPACITY 1.0f
#define FR_CHAR_HEAT_CAPACITY_TRANSITION_K 1800.0f
#define FR_CHAR_HEAT_CAPACITY_SLOPE (1.0f/650.0f)

#ifdef __STDC_VERSION__
#include <math.h>
#define FR_CHAR_SQRT sqrtf
static inline
#else
#define FR_CHAR_SQRT sqrt
#endif
float fr_char_specific_enthalpy(float temperature) {
    float above=temperature>FR_CHAR_HEAT_CAPACITY_TRANSITION_K?
        temperature-FR_CHAR_HEAT_CAPACITY_TRANSITION_K:0.f;
    return FR_CHAR_BASE_HEAT_CAPACITY*temperature+
        .5f*FR_CHAR_HEAT_CAPACITY_SLOPE*above*above;
}

#ifdef __STDC_VERSION__
static inline
#endif
float fr_char_mixture_temperature(float energy,float base_capacity,float char_mass) {
    if(base_capacity<=0.f) return 0.f;
    float transition=FR_CHAR_HEAT_CAPACITY_TRANSITION_K;
    if(char_mass<=0.f||energy<=base_capacity*transition) return energy/base_capacity;
    float excess=energy-base_capacity*transition;
    float strength=char_mass*FR_CHAR_HEAT_CAPACITY_SLOPE;
    float rise=2.f*excess/(base_capacity+FR_CHAR_SQRT(base_capacity*base_capacity+2.f*strength*excess));
    return transition+rise;
}

#ifdef __STDC_VERSION__
static inline
#endif
float fr_char_mixture_secant_capacity(float energy,float base_capacity,float char_mass) {
    float temperature=fr_char_mixture_temperature(energy,base_capacity,char_mass);
    return temperature>0.f?energy/temperature:base_capacity;
}

#undef FR_CHAR_SQRT
#endif
