#ifndef DUNGEONCRAFT_BULK_RHEOLOGY_LAYOUT_H
#define DUNGEONCRAFT_BULK_RHEOLOGY_LAYOUT_H

#define FR_BULK_RHEOLOGY_AMBIENT_K 293.0
#define FR_BULK_RHEOLOGY_GRAVITY 9.81
#define FR_BULK_RHEOLOGY_SHEAR_FLOOR 0.01
#define FR_BULK_RHEOLOGY_CAPACITY_FLOOR 0.000001
/* One cell is 1 cm^3. Density is g/cm^3; velocity is cells/s.
 * Convert normalized mass * velocity^2 to kJ of kinetic energy. */
#define FR_BULK_KINETIC_TO_KJ 1e-10

#endif
