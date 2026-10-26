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
layout(set=0,binding=26,std430) buffer OwnerTiles {uint owner_tiles[];};
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
bool gas_cell_owners() {return parameters[FR_GAS_CELL_OWNER_WORD]!=0u;}
uint gas_owner(uint atlas) {return gas_cell_owners()?(gates[atlas]&FR_GAS_CELL_OWNER_BIT)/FR_GAS_CELL_OWNER_BIT:params.bank&1u;}
Gas gas_source_at(uint atlas) {return gas_owner(atlas)==0u?bank0[atlas]:bank1[atlas];}
Gas gas_provisional(uint atlas) {return gas_owner(atlas)==0u?bank1[atlas]:bank0[atlas];}
void gas_write_source(uint atlas,Gas value) {if(gas_owner(atlas)==0u) bank0[atlas]=value;else bank1[atlas]=value;}
void gas_write_provisional(uint atlas,Gas value) {if(gas_owner(atlas)==0u) bank1[atlas]=value;else bank0[atlas]=value;}
Gas gas_world(uint atlas) {return gas_cell_owners()?gas_source_at(atlas):world[atlas];}
void gas_write_world(uint atlas,Gas value) {if(gas_cell_owners()) gas_write_source(atlas,value);else world[atlas]=value;}
Gas gas_source(ivec2 p) {uint at=gas_atlas(p);return at==0xffffffffu?gas_empty():gas_source_at(at);}
uint gas_cache_owner(uint index) {
    if(!gas_cell_owners()) return params.bank&1u;
    uint tile=(index/params.width/8u)*((params.width+7u)/8u)+(index%params.width/8u);
    return owner_tiles[tile]&1u;
}
Primitive gas_cache_source(uint index) {return gas_cache_owner(index)==0u?cache0[index]:cache1[index];}
Primitive gas_cache_candidate(uint index) {return gas_cache_owner(index)==0u?cache1[index]:cache0[index];}
void gas_cache_write_source(uint index,Primitive value) {if(gas_cache_owner(index)==0u) cache0[index]=value;else cache1[index]=value;}
void gas_cache_write_candidate(uint index,Primitive value) {if(gas_cache_owner(index)==0u) cache1[index]=value;else cache0[index]=value;}
Primitive gas_cached(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return gas_empty_primitive();
    uint at=uint(p.y)*params.width+uint(p.x),owner=params.bank&1u;
    if(gas_cell_owners()) owner=owner_tiles[(uint(p.y)/8u)*((params.width+7u)/8u)+uint(p.x)/8u]&1u;
    return owner==0u?cache0[at]:cache1[at];
}
float gas_cached_volume(ivec2 p) {
    uint index=uint(p.y)*params.width+uint(p.x),owner=params.bank&1u;
    if(gas_cell_owners()) owner=owner_tiles[(uint(p.y)/8u)*((params.width+7u)/8u)+uint(p.x)/8u]&1u;
    return owner==0u?cache0[index].metric.w:cache1[index].metric.w;
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
