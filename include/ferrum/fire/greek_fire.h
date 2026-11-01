#ifndef FERRUM_FIRE_GREEK_FIRE_H
#define FERRUM_FIRE_GREEK_FIRE_H

#include <stdbool.h>
#include <stdint.h>

#include "ferrum/fire/reactive_state.h"

typedef struct {
    uint32_t oil_fp, jelly_fp, bound_oxidizer_fp, ambient_oxygen_fp;
    uint32_t ash_fp, soot_fp, exhaust_fp;
    double released_heat, exported_energy;
} fr_coating_combustion_products_t;

/** React finite colocated oil and jelly film fuel without creating new inventory.
 * Mixed film uses its bound oxidizer before ambient air; water cannot supply
 * oxidizer. The ordinary oil control uses local ambient oxygen and douses. */
bool fr_coating_combustion_reference(const fr_reactive_state_t *source,
    uint32_t ambient_oxygen_fp,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_coating_combustion_products_t *products,
    char *err_buf,uint32_t err_cap);

#endif
