#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#include "types.glsl"
layout(local_size_x=64) in;
layout(set=0,binding=0,std430) restrict buffer World {Gas world[];};
layout(set=0,binding=1,std430) restrict buffer Bank0 {Gas bank0[];};
layout(set=0,binding=2,std430) restrict buffer Bank1 {Gas bank1[];};
layout(set=0,binding=3,std430) restrict buffer Cache0 {Primitive cache0[];};
layout(set=0,binding=4,std430) restrict buffer Cache1 {Primitive cache1[];};
layout(set=0,binding=5,std430) readonly buffer Model {uint parameters[];};
layout(set=0,binding=6,std430) coherent buffer Stats {uint stats[];};
layout(set=0,binding=7,std430) readonly buffer Pages {uint pages[];};
layout(set=0,binding=8,std430) readonly buffer Cells {uvec2 cells[];};
layout(set=0,binding=9,std430) readonly buffer Occupancy {uint occupancy[];};
layout(set=0,binding=10,std430) readonly buffer Mpm {vec4 mpm[];};
layout(set=0,binding=11,std430) restrict buffer Faces {GasFlux faces[];};
layout(push_constant) uniform Params {uint width,height,seconds,bank;} params;
#include "model.glsl"
uint gas_index() {return gl_GlobalInvocationID.x+gl_GlobalInvocationID.y*gl_NumWorkGroups.x*64u;}
uint gas_atlas(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return 0xffffffffu;
    uint page=pages[uint(p.y/64)*((params.width+63u)/64u)+uint(p.x/64)];
    return page==0u?0xffffffffu:(page-1u)*4096u+uint(p.y%64)*64u+uint(p.x%64);
}
Gas gas_source(ivec2 p) {
    uint at=gas_atlas(p);if(at==0xffffffffu) return gas_empty();
    return params.bank==0u?bank0[at]:bank1[at];
}
Primitive gas_cached(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return gas_empty_primitive();
    uint at=uint(p.y)*params.width+uint(p.x);return params.bank==0u?cache0[at]:cache1[at];
}
void gas_error(uint at) {atomicOr(stats[FR_GAS_ERROR_WORD],1u);atomicMin(stats[FR_GAS_ERROR_AT_WORD],at);}
void gas_rate(Primitive p) {
    float rate=abs(p.hydro.y)+abs(p.hydro.z)+2.*p.metric.x;
    uint bound=subgroupMax(gas_finite(rate)&&rate>=0.?floatBitsToUint(rate):0u);
    if(subgroupElect()&&bound!=0u) atomicMax(stats[FR_GAS_RATE_WORD],bound);
}
