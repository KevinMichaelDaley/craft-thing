#include "ferrum/fire/greek_fire.h"

bool fr_coating_combustion_reference(const fr_reactive_state_t *source,
    uint32_t ambient_oxygen_fp,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_coating_combustion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    (void)source;(void)ambient_oxygen_fp;(void)exposed_fraction;(void)seconds;
    (void)out;(void)products;(void)err_buf;(void)err_cap;
    return false;
}
