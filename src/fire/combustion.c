#include "ferrum/fire/combustion.h"
bool fr_combustion_reference(const fr_reactive_state_t *source,uint32_t oxygen_fp,
    float oxygen_fraction,float exposed_fraction,float seconds,
    fr_reactive_state_t *out,fr_combustion_products_t *products,
    char *err_buf,uint32_t err_cap) {
    (void)source;(void)oxygen_fp;(void)oxygen_fraction;(void)exposed_fraction;
    (void)seconds;(void)out;(void)products;(void)err_buf;(void)err_cap;
    return false;
}
