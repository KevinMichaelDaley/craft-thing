#include "storage.glsl"
layout(set=0,binding=11,std430) restrict buffer Acoustic {vec4 acoustic[];};
layout(set=0,binding=12,std430) restrict buffer Pressure {vec4 pressure[];};
layout(set=0,binding=13,std430) restrict buffer Stencil {vec4 stencil[];};
layout(set=0,binding=14,std430) restrict buffer Provisional {vec4 provisional[];};
layout(set=0,binding=15,std430) restrict buffer Correction {float correction[];};
layout(set=0,binding=16,std430) restrict buffer CorrectionData {vec4 correction_data[];};
layout(set=0,binding=17,std430) restrict buffer ActiveMask {uint active_mask[];};
layout(set=0,binding=18,std430) restrict buffer ActiveTiles {uint active_tiles[];};
layout(set=0,binding=19,std430) restrict buffer Indirect {uint indirect[];};
Gas gas_provisional(uint atlas) {return (params.bank&1u)==0u?bank1[atlas]:bank0[atlas];}
void gas_write_provisional(uint atlas,Gas value) {
    if((params.bank&1u)==0u) bank1[atlas]=value;else bank0[atlas]=value;
}
ivec2 gas_direction(uint i) {return i==0u?ivec2(1,0):(i==1u?ivec2(-1,0):(i==2u?ivec2(0,1):ivec2(0,-1)));}
bool gas_neighbor(ivec2 p,uint direction,out uint at) {
    ivec2 q=p+gas_direction(direction);uint atlas=gas_atlas(q);
    if(atlas==0xffffffffu) return false;
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
