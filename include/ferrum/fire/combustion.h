#ifndef FERRUM_FIRE_COMBUSTION_H
#define FERRUM_FIRE_COMBUSTION_H
#include "ferrum/fire/reactive_state.h"

/** Extensive products of one admitted ordinary substrate reaction. Char stays
 * with the parcel; ash and gas must be reserved before runtime publication. */
typedef struct {
    uint32_t fuel_fp,oxygen_fp,ash_fp,soot_fp,exhaust_fp;
    double released_heat,exported_energy;
} fr_combustion_products_t;
/** Cold reference for an exposed terrain parcel. Stable identity seeds its
 * integrated ignition threshold. Runtime uses parallel GPU reservations and
 * the same bounded reaction policy, never this function as a CPU field solve.
 * Failure changes neither output. No exposure means no reaction. */
bool fr_combustion_reference(const fr_reactive_state_t *source,uint32_t oxygen_fp,
    float oxygen_fraction,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_combustion_products_t *products,
    char *err_buf,uint32_t err_cap);
#endif
