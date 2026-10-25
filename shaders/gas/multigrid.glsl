#include "acoustic.glsl"
struct GasPressureNode {vec4 edges;vec4 data;};
layout(set=0,binding=24,std430) restrict buffer Multigrid {GasPressureNode hierarchy[];};
uvec2 gas_level_size(uint level) {
    uint scale=1u<<level;return (uvec2(params.width,params.height)+scale-1u)/scale;
}
uint gas_level_offset(uint level) {
    uint offset=0u;for(uint i=0u;i<level;i++) {uvec2 size=gas_level_size(i);offset+=size.x*size.y;}return offset;
}
uint gas_level_neighbor(uint index,uint direction,uint width) {
    return direction==0u?index+1u:(direction==1u?index-1u:(direction==2u?index+width:index-width));
}
float gas_level_residual(uint index,uint width) {
    GasPressureNode node=hierarchy[index];float value=node.data.y-node.data.x*node.data.z;
    for(uint d=0u;d<4u;d++) if(node.edges[d]!=0.) value+=node.edges[d]*hierarchy[gas_level_neighbor(index,d,width)].data.z;
    return value;
}
