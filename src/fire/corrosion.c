#include "ferrum/fire/corrosion.h"

bool fr_corrosion_reference(const fr_reactive_state_t *source,
    fr_corrosion_reagent_t reagent,uint32_t available_oxygen_fp,
    float oxygen_fraction,float saturation,float seconds,
    fr_reactive_state_t *out,fr_corrosion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    (void)source;(void)reagent;(void)available_oxygen_fp;(void)oxygen_fraction;
    (void)saturation;(void)seconds;(void)out;(void)products;
    (void)err_buf;(void)err_cap;
    return false;
}

uint32_t fr_corrosion_pristine_mass_fp(const fr_reactive_state_t *state) {
    (void)state;
    return 0u;
}
