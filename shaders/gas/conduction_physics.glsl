#ifndef FR_GAS_CONDUCTION_PHYSICS_GLSL
#define FR_GAS_CONDUCTION_PHYSICS_GLSL
#include "../../include/dungeoncraft/combustion_parameters.h"
#include "../../include/dungeoncraft/thermal_parameters.h"
#include "../../include/dungeoncraft/thermal_surface.h"
float gas_air_conductivity(float temperature) {
    /* Dry-air Sutherland conductivity, converted from cal/(cm s K) to
     * W/(m K).  Temperature is the mean across the conducting face. */
    float kelvin=max(temperature,1.);
    return 5.9776e-6*kelvin*sqrt(kelvin)/(kelvin+194.4)*418.4;
}
float gas_fourier_face_heat(float left_temperature,float right_temperature,
                            float left_capacity,float right_capacity,float aperture,float seconds) {
    if(left_capacity<=0.||right_capacity<=0.||aperture<=0.) return 0.;
    float difference=left_temperature-right_temperature;
    if(abs(difference)<=max(abs(left_temperature),abs(right_temperature))*1e-6) return 0.;
    /* Q = k A dt (T_left - T_right) / dx; bound each face by the explicit
     * four-neighbor capacity limit so a thin gas pocket cannot overshoot. */
    float conductance=gas_air_conductivity(.5*(left_temperature+right_temperature))
                     *float(FR_THERMAL_FACE_AREA/FR_THERMAL_GAMEPLAY_FACE_METERS)*aperture;
    float requested=conductance*difference*seconds;
    float stable=.25*left_capacity*right_capacity/(left_capacity+right_capacity)*abs(difference);
    return sign(requested)*min(abs(requested),stable);
}
#endif
