#ifndef FERRUM_FIRE_BULK_RHEOLOGY_H
#define FERRUM_FIRE_BULK_RHEOLOGY_H

#include <stdbool.h>
#include <stdint.h>

#include "ferrum/fire/material_profile.h"

/** Fraction of pressure-driven phase motion admitted by a yielded liquid.
 * Stress and shear rate are nonnegative cell-scale MPM quadrature samples. */
bool fr_bulk_rheology_mobility(const fr_reactive_profile_t *profile,
    float temperature_K,float stress,float shear_rate,float *out);

enum { FR_BULK_RHEOLOGY_MAX_QUADRATURE = 4 };
typedef struct {
    uint32_t volume_fp;
    float mass;
} fr_bulk_rheology_quadrature_t;

/** Generate stress quadrature weights from authoritative mobile Q16 phase volume.
 * Samples carry no independently stored inventory and may be regenerated. */
bool fr_bulk_rheology_quadrature(uint32_t volume_fp,float density,
    uint32_t sample_count,fr_bulk_rheology_quadrature_t *out);

#endif
