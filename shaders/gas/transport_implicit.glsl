#include "transport_float_controls.glsl"
#include "acoustic.glsl"
struct TransportCell {vec4 species,dynamics,visual,mass,energy;};
layout(set=0,binding=24,std430) restrict buffer TransportWork {TransportCell transport_work[];};
uint transport_axis() {return (params.bank>>3u)&1u;}
uint transport_position(ivec2 p) {return transport_axis()==0u?uint(p.y)*params.width+uint(p.x):uint(p.x)*params.height+uint(p.y);}
uint transport_line_start(ivec2 p) {return transport_axis()==0u?uint(p.y)*params.width:uint(p.x)*params.height;}
int transport_direction() {return (params.bank&16u)!=0u?-1:1;}
ivec2 transport_step() {return transport_axis()==0u?ivec2(transport_direction(),0):ivec2(0,transport_direction());}
vec2 transport_rates(ivec2 p,int direction) {
    ivec2 step=transport_axis()==0u?ivec2(direction,0):ivec2(0,direction);
    ivec2 q=p+step;Primitive own=gas_cached(p),other=gas_cached(q);
    if(own.metric.w<=0.||other.metric.w<=0.||!gas_gate_open(direction>0?p:q,transport_axis())) return vec2(0);
    float u=own.hydro[transport_axis()+1u],v=other.hydro[transport_axis()+1u],speed=max(abs(u),abs(v));
    float area=.5*min(own.metric.w,other.metric.w)/own.metric.w;
    return area*vec2(speed+float(direction)*u,speed+FR_GAS_LARGE_ENERGY_ADVECTION*float(direction)*u);
}
GasFlux transport_add(GasFlux a,GasFlux b) {
    a.species+=b.species;a.dynamics+=b.dynamics;a.pigment+=b.pigment;return a;
}
GasFlux transport_scale(GasFlux a,vec2 factor) {
    float energy=a.dynamics.y*factor.y;a.species*=factor.x;a.dynamics*=factor.x;a.dynamics.y=energy;a.pigment*=factor.x;return a;
}
GasFlux transport_state(Gas value) {return GasFlux(value.species,vec4(value.dynamics.x,gas_energy(value),value.dynamics.zw),value.visual.rgb);}
GasFlux transport_work_at(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return gas_empty_flux();
    TransportCell value=transport_work[transport_position(p)];return GasFlux(value.species,value.dynamics,value.visual.rgb);
}
vec4 transport_face_rates(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return vec4(0);
    return stencil[uint(p.y)*params.width+uint(p.x)];
}
GasFlux transport_face_flux(ivec2 p) {
    ivec2 step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);vec4 left=transport_face_rates(p),right=transport_face_rates(p+step);
    float dt=uintBitsToFloat(params.seconds);
    return transport_add(transport_scale(transport_work_at(p),dt*vec2(left.x,left.z)),transport_scale(transport_work_at(p+step),-dt*vec2(right.y,right.w)));
}
ivec2 transport_point(uint line,uint ordered) {
    uint length=transport_axis()==0u?params.width:params.height;
    uint cell=transport_direction()>0?ordered:length-1u-ordered;
    return transport_axis()==0u?ivec2(int(cell),int(line)):ivec2(int(line),int(cell));
}
