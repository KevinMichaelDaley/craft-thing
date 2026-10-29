#ifndef DC_COMBUSTION_PARAMETERS_H
#define DC_COMBUSTION_PARAMETERS_H
/* Effective airborne carbon/tar fuel uses the existing transported soot stock.
 * This chemical energy is reserved from primary heat, never added for free. */
#define FR_COMBUSTION_AIRBORNE_ENERGY 4000.0
#define FR_COMBUSTION_AIRBORNE_OXYGEN (8.0/3.0)
/* Effective voxel-average activation for fresh airborne organic tar. The
 * unresolved hot reaction front shares a cell with cooler entrained air. */
#define FR_COMBUSTION_AIRBORNE_IGNITION_K 500.0
#define FR_COMBUSTION_AIRBORNE_FULL_RATE_K 1100.0
#define FR_COMBUSTION_AIRBORNE_RATE 18.0
#define FR_COMBUSTION_AMBIENT_OXYGEN 0.21
/* A ten-micrometer char skin is optically visible on a centimeter voxel. */
#define FR_COMBUSTION_CHAR_OPTICAL_FRACTION 0.001
#define FR_COMBUSTION_SURFACE_GLOW_K 650.0
#define FR_COMBUSTION_SURFACE_EXPOSURE 1.0
#define FR_COMBUSTION_SMALL_BODY_MIN_REACTION_FP_PER_SECOND 16.0
#define FR_COMBUSTION_SMALL_BODY_FUEL_FP_LIMIT 16384u
#define FR_COMBUSTION_AIR_VISCOSITY 0.000018
#define FR_COMBUSTION_AIR_CONDUCTIVITY 0.02
#define FR_COMBUSTION_AIR_PRANDTL 0.71
#define FR_COMBUSTION_EMISSION_EXPOSURE 200.0
/* Effective soot density for a gray, centimeter-cell optical path. */
#define FR_COMBUSTION_SOOT_OPTICAL_DENSITY 0.00001
/* Bounded gameplay Arrhenius response, normalized at ignition. Fuel/O2 and
 * primary heat remain authoritative; no temperature clamp creates energy. */
#ifdef __STDC_VERSION__
#include <math.h>
static inline float fr_combustion_hot_rate(float temperature,float ignition) {
    return expf(fminf(1.38629436112f,6000.f*(1.f/ignition-1.f/fmaxf(temperature,1.f))));
}
#else
float fr_combustion_hot_rate(float temperature,float ignition) {
    return exp(min(1.38629436112,6000.*(1./ignition-1./max(temperature,1.))));
}
#endif
#endif
