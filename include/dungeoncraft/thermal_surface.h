#ifndef DC_THERMAL_SURFACE_H
#define DC_THERMAL_SURFACE_H
/* Centimeter-face boundary model. Radiance remains an energy transfer, never
 * a light or an extra heat reservoir. Bulk energy remains authoritative. */
#define FR_THERMAL_STEFAN_BOLTZMANN 5.670374419e-8
#define FR_THERMAL_FACE_AREA 0.0001
#define FR_THERMAL_SURFACE_EMISSIVITY 0.85
#define FR_THERMAL_CORE_DISTANCE 0.005
#define FR_THERMAL_RADIATION_RADIUS 8u
#ifdef __STDC_VERSION__
#include <math.h>
#define FR_SURFACE_MAX fmaxf
#define FR_SURFACE_SQRT sqrtf
static inline
#else
#define FR_SURFACE_MAX max
#define FR_SURFACE_SQRT sqrt
#endif
float fr_thermal_surface_K(float bulk,float gas,float core,float convection,float radiation,float incident) {
    float linear=core+convection,target=core*bulk+convection*gas+incident;
    if(radiation<=0.) return linear>0.?target/linear:bulk;
    float low=0.,high=FR_SURFACE_MAX(FR_SURFACE_MAX(bulk,gas),FR_SURFACE_SQRT(FR_SURFACE_SQRT(incident/radiation)));
    for(int iteration=0;iteration<16;iteration++) {
        float middle=.5*(low+high),square=middle*middle;
        if(radiation*square*square+linear*middle>target) high=middle;else low=middle;
    }
    return .5*(low+high);
}
#ifdef __STDC_VERSION__
static inline
#endif
float fr_surface_reaction_power(float previous,float energy,float seconds) {
    /* Optical skin response averages quantized, already debited reaction heat.
     * This is feedback only; it never credits the bulk energy reservoir. */
    return (previous*.25+energy)/(.25+seconds);
}
#ifdef __STDC_VERSION__
static inline
#endif
float fr_thermal_exposed_surface_K(float bulk,float gas,float core,float convection,float linked_radiation,float incident,float ambient) {
    /* Open air must not remove the emitting area from the boundary balance.
     * This estimates skin temperature only; transfers still debit real energy. */
    float missing=FR_SURFACE_MAX(0.,FR_THERMAL_STEFAN_BOLTZMANN*FR_THERMAL_FACE_AREA*FR_THERMAL_SURFACE_EMISSIVITY-linked_radiation);
    float square=ambient*ambient;
    return fr_thermal_surface_K(bulk,gas,core,convection,linked_radiation+missing,incident+missing*square*square);
}
#ifdef __STDC_VERSION__
static inline
#endif
float fr_thermal_heat_weight(float coverage,float capacity,float energy,float heat) {
    return coverage*FR_SURFACE_MAX(0.,heat<0.?energy:capacity);
}
#undef FR_SURFACE_MAX
#undef FR_SURFACE_SQRT
#endif
