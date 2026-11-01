#ifndef FERRUM_FIRE_COATING_H
#define FERRUM_FIRE_COATING_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/coating_layout.h"
#include "ferrum/fire/bulk_phase.h"

enum { FR_COATING_TERRAIN=1u, FR_COATING_RIGID=2u,
       FR_COATING_LEAF=3u, FR_COATING_ACTOR=4u };

/** Stable surface address. Rest coordinates follow rotation and streaming. */
typedef struct {
    uint32_t kind, id_low, id_high, generation, rest_x, rest_y;
} fr_coating_host_t;

/** Finite coating volume and phase heat relative to the ambient baseline. */
typedef struct {
    fr_coating_host_t host;
    uint32_t oil_fp, slime_fp;
    double excess_heat;
} fr_coating_film_t;

/** Contact wetting transfers mobile liquid and its proportional excess heat.
 * Invalid input leaves outputs untouched. Retained liquid cannot wet a host. */
bool fr_coating_contact_wet(const fr_bulk_cell_t *bulk,
    double bulk_excess_heat,const fr_coating_film_t *film,
    uint32_t phase,uint32_t requested_fp,fr_bulk_cell_t *bulk_out,
    double *bulk_excess_out,fr_coating_film_t *film_out,
    uint32_t *moved_fp);

/** Profile adhesion and film viscosity admit finite release under surface
 * traction. Heat and volume enter the receiving liquid only if space permits. */
bool fr_coating_release(const fr_coating_film_t *film,
    const fr_bulk_cell_t *receiver,double receiver_excess_heat,
    uint32_t phase,float temperature_K,float traction,float seconds,
    fr_coating_film_t *film_out,fr_bulk_cell_t *receiver_out,
    double *receiver_excess_out,uint32_t *released_fp);

/** Split a film across new stable rest-address owners after fracture. Each
 * phase volume and extensive heat is partitioned once, without duplication. */
bool fr_coating_film_split(const fr_coating_film_t *source,
    uint32_t left_weight_fp,const fr_coating_host_t *left_host,
    const fr_coating_host_t *right_host,fr_coating_film_t *left,
    fr_coating_film_t *right);

/** Convert one Q16 volume debit to integer species mass and retain the exact
 * sub-Q16 remainder. The packed word belongs to the same stable parcel. */
typedef struct {
    uint32_t oil_fuel_fp,jelly_fuel_fp,jelly_oxidizer_fp;
    uint32_t remainder_word;
} fr_coating_species_delta_t;
bool fr_coating_quantize_volume(uint32_t phase,uint32_t volume_fp,
    uint32_t remainder_word,fr_coating_species_delta_t *out);

#endif
