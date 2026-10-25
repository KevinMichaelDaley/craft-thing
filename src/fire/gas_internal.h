#ifndef FR_GAS_INTERNAL_H
#define FR_GAS_INTERNAL_H
#include <stddef.h>
#include <string.h>
#include "ferrum/fire/gas.h"
enum {GAS_WORDS=FR_GAS_SPECIES+6,GAS_ENERGY=FR_GAS_SPECIES,GAS_MOMENTUM=FR_GAS_SPECIES+1};
_Static_assert(sizeof(fr_gas_state_t)==GAS_WORDS*sizeof(double),"Gas reference field layout changed");
_Static_assert(offsetof(fr_gas_state_t,energy)==GAS_ENERGY*sizeof(double),"Gas energy layout changed");
_Static_assert(offsetof(fr_gas_state_t,momentum)==GAS_MOMENTUM*sizeof(double),"Gas momentum layout changed");
bool gas_error(char *err,uint32_t cap,const char *reason);
static inline fr_gas_state_t gas_combine(const fr_gas_state_t *a,double x,const fr_gas_state_t *b,double y) {
    double first[GAS_WORDS],second[GAS_WORDS],result[GAS_WORDS];fr_gas_state_t out;
    memcpy(first,a,sizeof(first));memcpy(second,b,sizeof(second));
    for(uint32_t i=0;i<GAS_WORDS;i++) result[i]=first[i]*x+second[i]*y;
    memcpy(&out,result,sizeof(out));return out;
}
#endif
