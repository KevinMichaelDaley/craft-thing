#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#include "types.glsl"
#ifndef FR_GAS_LOCAL_SIZE
#define FR_GAS_LOCAL_SIZE 64
#endif
layout(local_size_x=FR_GAS_LOCAL_SIZE) in;
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
#ifdef FR_GAS_WRITABLE_GATES
layout(set=0,binding=11,std430) buffer Gates {uint gates[];};
#else
layout(set=0,binding=11,std430) readonly buffer Gates {uint gates[];};
#endif
layout(push_constant) uniform Params {uint width,height,seconds,bank;} params;
#include "model.glsl"
uint gas_index() {return gl_GlobalInvocationID.x+gl_GlobalInvocationID.y*gl_NumWorkGroups.x*64u;}
uint gas_atlas(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return 0xffffffffu;
    uint page=pages[uint(p.y/64)*((params.width+63u)/64u)+uint(p.x/64)];
    return page==0u?0xffffffffu:(page-1u)*4096u+uint(p.y%64)*64u+uint(p.x%64);
}
bool gas_gate_open(ivec2 a,uint axis) {
    uint at=gas_atlas(a);return at==0xffffffffu||(gates[at]&(1u<<axis))==0u;
}
Gas gas_source(ivec2 p) {
    uint at=gas_atlas(p);if(at==0xffffffffu) return gas_empty();
    return (params.bank&1u)==0u?bank0[at]:bank1[at];
}
Primitive gas_cached(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return gas_empty_primitive();
    uint at=uint(p.y)*params.width+uint(p.x);return (params.bank&1u)==0u?cache0[at]:cache1[at];
}
void gas_error(uint at) {atomicOr(stats[FR_GAS_ERROR_WORD],1u);atomicMin(stats[FR_GAS_ERROR_AT_WORD],at);}
void gas_rate(Primitive p) {
    float rate=abs(p.hydro.y)+abs(p.hydro.z)+(!gas_implicit_policy()?2.*p.metric.x:0.);
    uint bound=subgroupMax(gas_finite(rate)&&rate>=0.?floatBitsToUint(rate):0u);
    if(subgroupElect()&&bound!=0u) atomicMax(stats[FR_GAS_RATE_WORD],bound);
    if(gas_implicit_policy()) {
        uint x=subgroupMax(floatBitsToUint(abs(p.hydro.y))),y=subgroupMax(floatBitsToUint(abs(p.hydro.z)));
        if(subgroupElect()) {if(x!=0u) atomicMax(stats[FR_GAS_RATE_X_WORD],x);if(y!=0u) atomicMax(stats[FR_GAS_RATE_Y_WORD],y);}
    }
}
