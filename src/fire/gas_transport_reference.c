#include "gas_internal.h"
bool fr_gas_transport_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,
    const double *v,uint32_t width,uint32_t height,double dt,fr_gas_state_t *out,char *err,uint32_t cap) {
    (void)m;(void)s;(void)v;(void)width;(void)height;(void)dt;(void)out;
    return gas_error(err,cap,"Large-CFL transport oracle is not implemented");
}
