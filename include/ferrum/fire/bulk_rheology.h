#ifndef FERRUM_FIRE_BULK_RHEOLOGY_H
#define FERRUM_FIRE_BULK_RHEOLOGY_H

#include <stdbool.h>

#include "ferrum/fire/material_profile.h"

/** Fraction of pressure-driven phase motion admitted by a yielded liquid.
 * Stress and shear rate are nonnegative cell-scale MPM quadrature samples. */
bool fr_bulk_rheology_mobility(const fr_reactive_profile_t *profile,
    float temperature_K,float stress,float shear_rate,float *out);

#endif
