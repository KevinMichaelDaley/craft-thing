#include "ferrum/fire/coating.h"

bool fr_coating_contact_wet(const fr_bulk_cell_t *bulk,
    double bulk_excess_heat,const fr_coating_film_t *film,
    uint32_t phase,uint32_t requested_fp,fr_bulk_cell_t *bulk_out,
    double *bulk_excess_out,fr_coating_film_t *film_out,
    uint32_t *moved_fp) {
    (void)bulk;(void)bulk_excess_heat;(void)film;(void)phase;
    (void)requested_fp;(void)bulk_out;(void)bulk_excess_out;
    (void)film_out;(void)moved_fp;
    return false;
}
