#ifndef FERRUM_FIRE_COMBUSTION_H
#define FERRUM_FIRE_COMBUSTION_H
#include "ferrum/fire/reactive_state.h"
#include "ferrum/fire/gas.h"

/** Extensive products of one admitted ordinary substrate reaction. Char stays
 * with the parcel; ash and gas must be reserved before runtime publication. */
typedef struct {
    uint32_t fuel_fp,ash_fp,soot_fp;
    /** Gas amounts use Q16 units but retain fractional quanta. */
    double oxygen_fp,exhaust_fp;
    double released_heat,exported_energy,reserved_airborne_heat;
} fr_combustion_products_t;
/** Cold reference for an exposed terrain parcel. Stable identity seeds its
 * integrated ignition threshold. Runtime uses parallel GPU reservations and
 * the same bounded reaction policy, never this function as a CPU field solve.
 * Failure changes neither output. No exposure means no reaction. */
bool fr_combustion_reference(const fr_reactive_state_t *source,double oxygen_fp,
    float oxygen_fraction,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_combustion_products_t *products,
    char *err_buf,uint32_t err_cap);
/** Cold reference for finite airborne carbon oxidation. This consumes existing
 * transported soot and oxygen, adding exhaust and its previously reserved heat.
 * Hot nonfuel gas and cold/oxygen-starved carbon do not react. */
bool fr_combustion_gas_reference(const fr_gas_model_t *model,const fr_gas_state_t *source,
    double volume,double seconds,fr_gas_state_t *out,double *reacted,
    char *err_buf,uint32_t err_cap);
#endif
