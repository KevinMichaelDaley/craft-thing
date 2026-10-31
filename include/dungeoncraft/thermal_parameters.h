#ifndef DC_THERMAL_PARAMETERS_H
#define DC_THERMAL_PARAMETERS_H

/* Shared C/GLSL game-model parameters. Heat sources are finite reservoirs;
 * the first bench stays below boiling by its source temperature, not a clamp. */
#define FR_THERMAL_AMBIENT_K 293.15
#define FR_THERMAL_PROBE_K 350.0
#define FR_THERMAL_PROBE_CAPACITY 200.0
#define FR_THERMAL_PROBE_CONDUCTIVITY 20.0
#define FR_THERMAL_WETTING_PER_SECOND 0.06
#define FR_THERMAL_NEIGHBORS 4u
/* Gameplay pixels represent centimeter cubes: face area / face separation
 * is one centimeter. The delivered laboratory bench keeps unit geometry. */
#define FR_THERMAL_GAMEPLAY_FACE_METERS 0.01
#define FR_THERMAL_2D_TRANSPORT_SCALE 1.0

#endif
