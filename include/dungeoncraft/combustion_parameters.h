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
#define FR_COMBUSTION_AIR_VISCOSITY 0.000018
#define FR_COMBUSTION_AIR_CONDUCTIVITY 0.02
#define FR_COMBUSTION_AIR_PRANDTL 0.71
#define FR_COMBUSTION_EMISSION_EXPOSURE 200.0
#endif
