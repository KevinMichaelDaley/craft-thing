#ifndef FERRUM_FIRE_GAS_CONSTANTS_H
#define FERRUM_FIRE_GAS_CONSTANTS_H
/* Unsplit Rusanov outgoing species weights sum to at most
 * dt * (global_max_abs_vx + global_max_abs_vy). */
#define FR_GAS_ADVECTIVE_COURANT 0.9
#define FR_GAS_LARGE_ENERGY_ADVECTION 0.5
/* Random close packing of round grains leaves connected three-dimensional
 * pore space even when their projected MPM grid density exceeds one. */
#define FR_GAS_GRANULAR_PACKING_FRACTION 0.64
#endif
