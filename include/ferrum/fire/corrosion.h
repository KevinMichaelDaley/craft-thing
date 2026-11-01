#ifndef FERRUM_FIRE_CORROSION_H
#define FERRUM_FIRE_CORROSION_H

#include <stdbool.h>
#include <stdint.h>

#include "ferrum/fire/reactive_state.h"

typedef enum {
    FR_CORROSION_AIR = 0,
    FR_CORROSION_GREEK_FIRE = 1
} fr_corrosion_reagent_t;

typedef struct {
    uint32_t converted_fp;
    uint32_t residue_fp;
    uint32_t oxidizer_fp;
    float section_strength_fraction;
} fr_corrosion_products_t;

/** Reference transaction for ordinary oxidation or finite magical corrosion.
 * Greek-fire exposure draws bound oxidizer from the coating; ambient oxygen is
 * drawn only by ordinary oxidation. Invalid input changes neither output. */
bool fr_corrosion_reference(const fr_reactive_state_t *source,
    fr_corrosion_reagent_t reagent,uint32_t available_oxygen_fp,
    float oxygen_fraction,float saturation,float seconds,
    fr_reactive_state_t *out,fr_corrosion_products_t *products,
    char *err_buf,uint32_t err_cap);

/** Pristine substrate inventory remaining after partial conversion. */
uint32_t fr_corrosion_pristine_mass_fp(const fr_reactive_state_t *state);

#endif
