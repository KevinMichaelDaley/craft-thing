#extension GL_GOOGLE_include_directive : require
#extension GL_KHR_shader_subgroup_shuffle_relative : require
#include "transport_implicit.glsl"
shared vec4 group_mass[64],group_energy[64];
shared float group_peak[64];
vec4 matrix_product(vec4 a,vec4 b) {
    precise vec4 value=vec4(fma(a.x,b.x,a.y*b.z),fma(a.x,b.y,a.y*b.w),fma(a.z,b.x,a.w*b.z),fma(a.z,b.y,a.w*b.w));
    float scale=max(max(abs(value.x),abs(value.y)),max(abs(value.z),abs(value.w)));
    return scale>0.?value/scale:value;
}
void coefficients(ivec2 p,float dt,out vec2 lower,out vec2 diagonal,out vec2 upper,out vec2 previous_upper,out vec4 face) {
    ivec2 step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
    vec2 plus=transport_rates(p,1),minus=transport_rates(p,-1);
    lower=dt*transport_rates(p-step,1);diagonal=vec2(1)+dt*(plus+minus);upper=dt*transport_rates(p+step,-1);previous_upper=dt*minus;
    face=vec4(plus.x,minus.x,plus.y,minus.y);
}
void main() {
    uint lane=gl_SubgroupInvocationID,rank=gl_SubgroupID*gl_SubgroupSize+lane;
    uint length=transport_axis()==0u?params.width:params.height,span=(length+FR_GAS_LOCAL_SIZE-1u)/FR_GAS_LOCAL_SIZE;
    uint begin=rank*span,end=min(begin+span,length),line=gl_WorkGroupID.x;float dt=uintBitsToFloat(params.seconds);
    float maximum_speed=0.;
    for(uint cell=begin;cell<end;cell++) maximum_speed=max(maximum_speed,abs(gas_cached(transport_point(line,cell)).hydro[transport_axis()+1u]));
    float subgroup_speed=subgroupMax(maximum_speed);
    if(lane+1u==gl_SubgroupSize) group_peak[gl_SubgroupID]=subgroup_speed;
    barrier();maximum_speed=0.;
    for(uint group=0u;group<gl_NumSubgroups;group++) maximum_speed=max(maximum_speed,group_peak[group]);
    if(maximum_speed==0.) {if(rank==0u) transport_work[line*length].mass.w=-1.;return;}
    barrier();
    vec4 mass=vec4(1,0,0,1),energy=mass;float peak=0.;
    for(uint cell=begin;cell<end;cell++) {
        ivec2 p=transport_point(line,cell);vec2 lower,diagonal,upper,previous;vec4 face;coefficients(p,dt,lower,diagonal,upper,previous,face);
        uint index=uint(p.y)*params.width+uint(p.x);stencil[index]=face;
        uint atlas=gas_atlas(p);GasFlux value=atlas==0xffffffffu?gas_empty_flux():transport_state(gas_provisional(atlas));
        peak=max(peak,max(value.species.z,value.dynamics.x));
        transport_work[transport_position(p)]=TransportCell(value.species,value.dynamics,
            vec4(value.pigment,any(notEqual(face,vec4(0)))?1.:0.),
            vec4(lower.x,diagonal.x,upper.x,previous.x),vec4(lower.y,diagonal.y,upper.y,previous.y));
        mass=matrix_product(vec4(diagonal.x,-lower.x*previous.x,1,0),mass);
        energy=matrix_product(vec4(diagonal.y,-lower.y*previous.y,1,0),energy);
    }
    for(uint offset=1u;offset<gl_SubgroupSize;offset*=2u) {
        vec4 prior_mass=subgroupShuffleUp(mass,offset),prior_energy=subgroupShuffleUp(energy,offset);
        if(lane>=offset) {mass=matrix_product(mass,prior_mass);energy=matrix_product(energy,prior_energy);}
    }
    float subgroup_peak=subgroupMax(peak);
    if(lane+1u==gl_SubgroupSize) {group_mass[gl_SubgroupID]=mass;group_energy[gl_SubgroupID]=energy;group_peak[gl_SubgroupID]=subgroup_peak;}
    vec4 exclusive_mass=subgroupShuffleUp(mass,1u),exclusive_energy=subgroupShuffleUp(energy,1u);
    if(lane==0u) {exclusive_mass=vec4(1,0,0,1);exclusive_energy=exclusive_mass;}
    barrier();mass=vec4(1,0,0,1);energy=mass;
    peak=0.;for(uint group=0u;group<gl_NumSubgroups;group++) peak=max(peak,group_peak[group]);
    float trace_scale=peak<=3.402823466e38/(float(length+1u)*4294967296.*16.)?4294967296.:1.;
    for(uint group=0u;group<gl_SubgroupID;group++) {mass=matrix_product(group_mass[group],mass);energy=matrix_product(group_energy[group],energy);}
    mass=matrix_product(exclusive_mass,mass);energy=matrix_product(exclusive_energy,energy);
    vec2 prior_inverse=vec2(mass.z/mass.x,energy.z/energy.x);
    for(uint cell=begin;cell<end;cell++) {
        ivec2 p=transport_point(line,cell);uint index=uint(p.y)*params.width+uint(p.x);
        TransportCell stored=transport_work[transport_position(p)];vec4 saved_mass=stored.mass,saved_energy=stored.energy;
        vec2 lower=vec2(saved_mass.x,saved_energy.x),diagonal=vec2(saved_mass.y,saved_energy.y);
        vec2 upper=vec2(saved_mass.z,saved_energy.z),previous=vec2(saved_mass.w,saved_energy.w);
        vec2 inverse=vec2(1)/(diagonal-lower*previous*prior_inverse);
        if(any(lessThanEqual(inverse,vec2(0)))||any(isnan(inverse))||any(isinf(inverse))) {gas_error(index);atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],FR_GAS_REJECT_FACTOR);inverse=vec2(0);}
        GasFlux value=GasFlux(stored.species,stored.dynamics,stored.visual.rgb);
        value.species.z*=trace_scale;value.dynamics.x*=trace_scale;value.pigment*=trace_scale;
        transport_work[transport_position(p)]=TransportCell(value.species,value.dynamics,vec4(value.pigment,0),
            vec4(lower.x*inverse.x,inverse.x,upper.x*inverse.x,trace_scale),
            vec4(lower.y*inverse.y,inverse.y,upper.y*inverse.y,stored.visual.w));
        prior_inverse=inverse;
    }
}
