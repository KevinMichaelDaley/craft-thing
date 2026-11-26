#ifndef FR_GAS_TYPES_GLSL
#define FR_GAS_TYPES_GLSL
#include "../../include/dungeoncraft/gas_layout.h"
/* The trailing pair carries the low words of exhaust and soot through publication. */
struct Gas {vec4 species;vec4 dynamics;vec4 visual;vec4 geometry;vec2 carrier_tail;};
/* Hydro: density,u,v,pressure. Metric: sound/enthalpy,temperature/pressure
 * capacity,mass,void; the implicit policy caches its sufficient thermodynamics. */
struct Primitive {vec4 hydro;vec4 metric;};
struct GasFlux {vec4 species;vec4 dynamics;vec3 pigment;};
Gas gas_empty() {return Gas(vec4(0),vec4(0),vec4(0),vec4(0),vec2(0));}
Primitive gas_empty_primitive() {return Primitive(vec4(0),vec4(0));}
GasFlux gas_empty_flux() {return GasFlux(vec4(0),vec4(0),vec3(0));}
vec2 gas_two_sum(float a,float b) {
    precise float sum=a+b;
    precise float recovered=sum-a;
    precise float error=(a-(sum-recovered))+(b-recovered);
    return vec2(sum,error);
}
vec2 gas_add_stock(vec2 stock,float change) {
    if(change==0.) return stock;
    vec2 sum=gas_two_sum(stock.x,change);
    precise float carry=stock.y+sum.y;
    return gas_two_sum(sum.x,carry);
}
vec2 gas_scale_stock(vec2 stock,float fraction) {
    precise float high=stock.x*fraction;
    precise float low=fma(stock.x,fraction,-high)+stock.y*fraction;
    return gas_two_sum(high,low);
}
vec2 gas_exhaust_stock(Gas value) {return vec2(value.species.z,value.carrier_tail.x);}
vec2 gas_soot_stock(Gas value) {return vec2(value.dynamics.x,value.carrier_tail.y);}
Gas gas_set_carriers(Gas value,vec2 exhaust,vec2 soot) {
    value.species.z=exhaust.x;
    value.dynamics.x=soot.x;
    value.carrier_tail=vec2(exhaust.y,soot.y);
    return value;
}
Gas gas_add_carriers(Gas value,vec2 change) {
    return gas_set_carriers(value,gas_add_stock(gas_exhaust_stock(value),change.x),
        gas_add_stock(gas_soot_stock(value),change.y));
}
Gas gas_add_carrier_stocks(Gas value,vec2 exhaust,vec2 soot) {
    value=gas_add_carriers(value,vec2(exhaust.x,soot.x));
    return gas_add_carriers(value,vec2(exhaust.y,soot.y));
}
#endif
