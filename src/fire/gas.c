#include <stdio.h>
#include "ferrum/fire/gas.h"

static bool pending(char *err,uint32_t cap) {
    if(err&&cap) snprintf(err,cap,"Gas numerical reference is not implemented");
    return false;
}
bool fr_gas_thermodynamics(const fr_gas_model_t *m,const fr_gas_state_t *s,
    double v,fr_gas_thermo_t *out,char *err,uint32_t cap) {
    (void)m;(void)s;(void)v;(void)out;return pending(err,cap);
}
bool fr_gas_reference_compress(const fr_gas_model_t *m,const fr_gas_state_t *s,
    double a,double b,fr_gas_state_t *out,double *work,char *err,uint32_t cap) {
    (void)m;(void)s;(void)a;(void)b;(void)out;(void)work;return pending(err,cap);
}
bool fr_gas_admit(const fr_gas_model_t *m,const fr_gas_state_t *s,const fr_gas_state_t *t,
    double f,fr_gas_state_t *so,fr_gas_state_t *to,char *err,uint32_t cap) {
    (void)m;(void)s;(void)t;(void)f;(void)so;(void)to;return pending(err,cap);
}
bool fr_gas_reference_max_dt(const fr_gas_model_t *m,const fr_gas_state_t *s,const double *v,
    uint32_t w,uint32_t h,double dx,double c,double *out,char *err,uint32_t cap) {
    (void)m;(void)s;(void)v;(void)w;(void)h;(void)dx;(void)c;(void)out;return pending(err,cap);
}
bool fr_gas_reference_step(const fr_gas_model_t *m,const fr_gas_state_t *s,const double *v,
    uint32_t w,uint32_t h,double dx,double dt,uint32_t solver,fr_gas_state_t *out,char *err,uint32_t cap) {
    (void)m;(void)s;(void)v;(void)w;(void)h;(void)dx;(void)dt;(void)solver;(void)out;return pending(err,cap);
}
