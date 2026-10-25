#ifndef FR_GAS_TYPES_GLSL
#define FR_GAS_TYPES_GLSL
#include "../../include/dungeoncraft/gas_layout.h"
struct Gas {vec4 species;vec4 dynamics;vec4 visual;vec4 geometry;};
struct Primitive {vec4 hydro;vec4 metric;};
struct GasFlux {vec4 species;vec4 dynamics;vec3 pigment;};
Gas gas_empty() {return Gas(vec4(0),vec4(0),vec4(0),vec4(0));}
Primitive gas_empty_primitive() {return Primitive(vec4(0),vec4(0));}
GasFlux gas_empty_flux() {return GasFlux(vec4(0),vec4(0),vec3(0));}
#endif
