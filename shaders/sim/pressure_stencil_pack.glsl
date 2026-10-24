#ifndef DC_PRESSURE_STENCIL_PACK_GLSL
#define DC_PRESSURE_STENCIL_PACK_GLSL
uvec4 pack_pressure_stencil(uvec4 weights,uint divergence,bool cached) {return weights;}
uint unpack_pressure_weight(uvec4 packed,int axis) {return packed[axis];}
bool pressure_divergence_cached(uvec4 packed) {return false;}
uint unpack_pressure_divergence(uvec4 packed) {return 0u;}
#endif
