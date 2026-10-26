#include "acoustic.glsl"
void main() {
#ifdef FR_GAS_LAZY_PRESSURE_CLEAR
    uint tile=active_tiles[3u*gas_tile_count()+gl_WorkGroupID.x];
    if(gl_WorkGroupID.x==0u&&gl_LocalInvocationIndex==0u) atomicOr(stats[15],524288u);
    if((activity[gas_tile_count()+tile]&1u)!=0u) return;
#endif
    uint index=gas_sparse_index(3u);if(index>=params.width*params.height) return;
    pressure[index]=vec4(0);acoustic[index].z=0.;correction[index]=0.;correction_data[index]=vec4(0);
}
