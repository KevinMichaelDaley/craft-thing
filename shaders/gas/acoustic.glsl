#define FR_GAS_FORCE_IMPLICIT 1
#include "storage.glsl"
layout(set=0,binding=12,std430) restrict buffer Acoustic {vec4 acoustic[];};
layout(set=0,binding=13,std430) restrict buffer Pressure {vec4 pressure[];};
layout(set=0,binding=14,std430) restrict buffer Stencil {vec4 stencil[];};
layout(set=0,binding=15,std430) restrict buffer Provisional {vec4 provisional[];};
layout(set=0,binding=16,std430) restrict buffer Correction {float correction[];};
layout(set=0,binding=17,std430) restrict buffer CorrectionData {vec4 correction_data[];};
layout(set=0,binding=18,std430) restrict buffer ActiveMask {uint active_mask[];};
layout(set=0,binding=19,std430) restrict buffer ActiveTiles {uint active_tiles[];};
layout(set=0,binding=20,std430) restrict buffer Force {vec2 force[];};
layout(set=0,binding=21,std430) restrict buffer Activity {uint activity[];};
layout(set=0,binding=22,std430) restrict buffer Matrix {vec4 matrix[];};
layout(set=0,binding=23,std430) restrict buffer Indirect {uint indirect[];};
uint gas_tile_count() {return ((params.width+7u)/8u)*((params.height+7u)/8u);}
vec2 gas_large_pressure_state(Gas value,float old_pressure) {
    precise float mass=(((value.species.x+value.species.y)+value.species.z)+value.species.w)+value.dynamics.x;
    precise float energy=(value.dynamics.y+value.visual.w)+value.geometry.z;
    if(gas_practical_policy()) energy=gas_thermal_energy(value);
    float fraction=gas_practical_policy()?1.:1.-FR_GAS_LARGE_ENERGY_ADVECTION;
    precise float numerator=fraction*energy+gas_parameter(FR_GAS_KINETIC_WORD)*old_pressure*value.geometry.x;
    return mass>0.?vec2(mass/value.geometry.x,numerator/mass):vec2(0);
}
float gas_pressure_relaxation(vec4 coefficients,float diagonal) {
    if(gas_practical_policy()) return 1.8;
    precise float total=((coefficients.x+coefficients.y)+coefficients.z)+coefficients.w;
    precise float ratio=diagonal>0.?min(total/diagonal,.99999994):0.;
    precise float remainder=1.-ratio*ratio;
    precise float denominator=1.+sqrt(max(0.,remainder));
    return 2./denominator;
}
bool gas_pressure_accepted() {return stats[FR_GAS_ERROR_WORD]==0u&&(gas_practical_policy()||uintBitsToFloat(stats[FR_GAS_RESIDUAL_WORD])<=1e-7);}
bool gas_advected(uint index) {
    uint tile=(index/params.width/8u)*((params.width+7u)/8u)+(index%params.width/8u);
    return (activity[gas_tile_count()+tile]&1u)!=0u;
}
Primitive gas_pressure_stage(ivec2 p,uint index) {
    if(gas_advected(index)) return (params.bank&1u)==0u?cache1[index]:cache0[index];
    Primitive own=gas_cached(p);vec2 stage=gas_large_pressure_state(gas_source(p),own.hydro.w);
    own.hydro.x=stage.x;own.metric.x=stage.y;return own;
}
uint gas_sparse_index(uint section) {
    uint tile=active_tiles[section*gas_tile_count()+gl_WorkGroupID.x],lane=gl_LocalInvocationIndex;
    uint width=(params.width+7u)/8u;
    uvec2 p=uvec2(tile%width,tile/width)*8u+uvec2(lane%8u,lane/8u);
    return p.x<params.width&&p.y<params.height?p.y*params.width+p.x:0xffffffffu;
}
Gas gas_provisional(uint atlas) {return (params.bank&1u)==0u?bank1[atlas]:bank0[atlas];}
void gas_write_provisional(uint atlas,Gas value) {
    if((params.bank&1u)==0u) bank1[atlas]=value;else bank0[atlas]=value;
}
ivec2 gas_direction(uint i) {return i==0u?ivec2(1,0):(i==1u?ivec2(-1,0):(i==2u?ivec2(0,1):ivec2(0,-1)));}
bool gas_neighbor(ivec2 p,uint direction,out uint at) {
    ivec2 q=p+gas_direction(direction);uint atlas=gas_atlas(q);
    if(atlas==0xffffffffu) return false;
    if(!gas_gate_open((direction&1u)==0u?p:q,direction/2u)) return false;
    at=uint(q.y)*params.width+uint(q.x);return gas_cached(q).metric.w>0.;
}
float gas_specific_capacity(Gas s) {
    float cv=dot(s.species,vec4(gas_parameter(5u),gas_parameter(6u),gas_parameter(7u),gas_parameter(8u)))+s.dynamics.x*gas_parameter(9u);
    float molecular=dot(s.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+s.dynamics.x*gas_parameter(4u);
    return molecular>0.?gas_parameter(FR_GAS_KINETIC_WORD)*s.geometry.x*cv/molecular:0.;
}
float gas_kinetic(Gas s) {
    float mass=gas_mass(s);return mass>0.?.5*gas_parameter(FR_GAS_KINETIC_WORD)*dot(s.dynamics.zw,s.dynamics.zw)/mass:0.;
}
vec2 gas_pressure_product(vec2 sum,float a,float b) {
    precise float product=a*b;precise float residual=fma(a,b,-product);
    vec2 first=gas_two_sum(sum.x,product),tail=gas_two_sum(sum.y,first.y+residual);
    vec2 top=gas_two_sum(first.x,tail.x);return vec2(top.x,top.y+tail.y);
}
float gas_pressure_residual(uint index) {
    vec2 sum=vec2(pressure[index].y,0);
    sum=gas_pressure_product(sum,-pressure[index].z,pressure[index].x);
    sum=gas_pressure_product(sum,-pressure[index].z,acoustic[index].z);
    for(uint d=0u;d<4u;d++) {
        if(stencil[index][d]==0.) continue;
        uint j=d==0u?index+1u:(d==1u?index-1u:(d==2u?index+params.width:index-params.width));
        sum=gas_pressure_product(sum,stencil[index][d],pressure[j].x);
        sum=gas_pressure_product(sum,stencil[index][d],acoustic[j].z);
    }
    return sum.x+sum.y;
}
bool gas_pressure_row_converged(uint index) {
    precise float high=pressure[index].z*pressure[index].x,low=pressure[index].z*acoustic[index].z;
    precise float residual=(pressure[index].y-high)-low;
    float magnitude=abs(pressure[index].y)+abs(high)+abs(low);
    for(uint d=0u;d<4u;d++) {
        float coefficient=stencil[index][d];if(coefficient==0.) continue;
        uint j=d==0u?index+1u:(d==1u?index-1u:(d==2u?index+params.width:index-params.width));
        precise float first=coefficient*pressure[j].x,second=coefficient*acoustic[j].z;
        residual=(residual+first)+second;magnitude+=abs(first)+abs(second);
    }
    /* A conservative forward-error bound skips already-converged rows only.
     * Every sweep revisits them, so a changed neighbor wakes the row. */
    return abs(residual)+0.000003814697265625*magnitude<=1e-7*pressure[index].w;
}
