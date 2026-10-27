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
static inline
#endif
float fr_thermal_surface_K(float bulk,float gas,float core,float convection,float radiation,float incident) {
    (void)gas;(void)core;(void)convection;(void)radiation;(void)incident;
    return bulk;
}
#endif
