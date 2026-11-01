#ifndef FERRUM_FIRE_COATING_H
#define FERRUM_FIRE_COATING_H

#include <stdbool.h>
#include <stdint.h>

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

#endif
