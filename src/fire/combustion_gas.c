#include "ferrum/fire/combustion.h"
bool fr_combustion_gas_reference(const fr_gas_model_t *model,const fr_gas_state_t *source,
    double volume,double seconds,fr_gas_state_t *out,double *reacted,char *err,uint32_t cap) {
    (void)model;(void)source;(void)volume;(void)seconds;(void)out;(void)reacted;(void)err;(void)cap;
    return false;
}
