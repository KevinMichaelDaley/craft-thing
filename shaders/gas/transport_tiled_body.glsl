#define FR_GAS_CUSTOM_LOCAL_SIZE 1
#define FR_GAS_COMPACT_WORK 1
#include "transport_implicit.glsl"
#include "transport_apply_body.glsl"
layout(local_size_x=256) in;
/* Eight stages need at most sixteen halo cells along one transport axis. */
shared Gas tile_state[160];
shared vec4 tile_rates[160];
shared float tile_peak[64];
uint tile_position(uint along,uint across,uint extent) {return across*extent+along;}
ivec2 tile_point(uint at,uint extent,ivec2 origin) {
    ivec2 offset=ivec2(at%extent,at/extent);
    return origin+(transport_axis()==0u?offset:offset.yx);
}
GasFlux tile_value(uint at,float scale) {
    GasFlux value=transport_state(tile_state[at]);value.species.z*=scale;
    value.dynamics.x*=scale;value.pigment*=scale;return value;
}
GasFlux tile_face(uint left,uint right,float seconds,float scale) {
    return transport_add(transport_scale(tile_value(left,scale),seconds*tile_rates[left].xy),
        transport_scale(tile_value(right,scale),-seconds*tile_rates[right].zw));
}
void main() {
    uint rank=gl_LocalInvocationIndex,stages=(params.bank>>20u)&15u,extent=64u+2u*stages,count=extent*2u;
    ivec2 base=ivec2(gl_WorkGroupID.xy)*(transport_axis()==0u?ivec2(64,2):ivec2(2,64));
    ivec2 origin=base-(transport_axis()==0u?ivec2(stages,0):ivec2(0,stages));
    float peak=0.;
    for(uint at=rank;at<count;at+=256u) {
        ivec2 p=tile_point(at,extent,origin);uint atlas=gas_atlas(p);
        Gas value=atlas==0xffffffffu?gas_empty():(transport_axis()==0u?gas_source(p):gas_provisional(atlas));
        tile_state[at]=value;tile_rates[at]=vec4(transport_rates(p,1),transport_rates(p,-1));
        peak=max(peak,max(value.species.z,value.dynamics.x));
    }
    peak=subgroupMax(peak);
    if(gl_SubgroupInvocationID==0u) tile_peak[gl_SubgroupID]=peak;
    barrier();peak=0.;
    for(uint i=0u;i<gl_NumSubgroups;i++) peak=max(peak,tile_peak[i]);
    float length=float(transport_axis()==0u?params.width:params.height);
    float scale=peak<=3.402823466e38/((length+1.)*4294967296.*16.)?4294967296.:1.;
    float seconds=uintBitsToFloat(params.seconds)/float(stages);
    if(all(equal(gl_WorkGroupID,uvec3(0)))&&rank==0u)
        atomicOr(stats[15],(4u<<transport_axis())|(stages<<(8u+transport_axis()*4u)));
    for(uint stage=0u;stage<stages;stage++) {
        Gas next;uint along=rank%extent;
        if(rank<count&&along>stage&&along+stage+1u<extent) {
            GasFlux incoming=tile_face(rank-1u,rank,seconds,scale),outgoing=tile_face(rank,rank+1u,seconds,scale);
            next=transport_update_state(tile_state[rank],incoming,outgoing,scale);
        }
        barrier();
        if(rank<count&&along>stage&&along+stage+1u<extent) tile_state[rank]=next;
        barrier();
    }
    if(rank>=128u) return;
    ivec2 local=ivec2(rank%64u,rank/64u);
    uint along=uint(local.x)+stages,across=uint(local.y);
    ivec2 p=base+(transport_axis()==0u?local:local.yx);uint atlas=gas_atlas(p);
    if(atlas!=0xffffffffu) {
        Gas value=tile_state[tile_position(along,across,extent)];
        if(transport_axis()==0u) gas_write_provisional(atlas,value);else world[atlas]=value;
    }
}
