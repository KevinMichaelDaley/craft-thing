#define FR_GAS_COMPACT_WORK 1
#define FR_GAS_FORCE_PRACTICAL 1
#include "transport_implicit.glsl"
#include "transport_apply_body.glsl"
#ifndef FR_GAS_REUSE_LINE_SHARED
shared vec4 line_species[FR_GAS_EXPLICIT_LINE_CAPACITY];
#endif
shared vec4 line_dynamics[FR_GAS_EXPLICIT_LINE_CAPACITY];
shared vec4 line_visual[FR_GAS_EXPLICIT_LINE_CAPACITY];
shared float explicit_group_peak[64];
float line_maximum(float value) {
    float peak=subgroupMax(value);
    if(gl_SubgroupInvocationID==0u) explicit_group_peak[gl_SubgroupID]=peak;
    barrier();peak=0.;
    for(uint group=0u;group<gl_NumSubgroups;group++) peak=max(peak,explicit_group_peak[group]);
    barrier();return peak;
}
GasFlux line_source(ivec2 p) {
    if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(params.width,params.height)))) return gas_empty_flux();
    uint cell=transport_axis()==0u?uint(p.x):uint(p.y);
    return GasFlux(line_species[cell],line_dynamics[cell],line_visual[cell].rgb);
}
GasFlux line_face_flux(ivec2 p,float seconds) {
    ivec2 step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
    vec2 own=transport_rates(p,1),neighbor=transport_rates(p+step,-1);
    return transport_add(transport_scale(line_source(p),seconds*own),transport_scale(line_source(p+step),-seconds*neighbor));
}
#ifndef FR_GAS_EXPLICIT_ENTRY
#define FR_GAS_EXPLICIT_ENTRY main
#endif
#ifdef FR_GAS_LOCAL_STAGES
#include "transport_explicit_local_body.glsl"
#else
void FR_GAS_EXPLICIT_ENTRY() {
    uint rank=gl_LocalInvocationIndex,length=transport_axis()==0u?params.width:params.height,line=gl_WorkGroupID.x;
    if(rank==0u) transport_work[line*length].mass.w=0.;
    if(length>FR_GAS_EXPLICIT_LINE_CAPACITY) {if(rank==0u) atomicAdd(stats[FR_GAS_IMPLICIT_LINE_COUNT_WORD],1u);return;}
    float speed=0.;
    for(uint cell=rank;cell<length;cell+=FR_GAS_LOCAL_SIZE)
        speed=max(speed,abs(gas_cached(transport_point(line,cell)).hydro[transport_axis()+1u]));
    speed=line_maximum(speed);
    if(speed==0.) {
        if(rank==0u) {transport_work[line*length].mass.w=-1.;atomicAdd(stats[FR_GAS_INACTIVE_LINE_COUNT_WORD],1u);}return;
    }
    float seconds=uintBitsToFloat(params.seconds);
    float courant=seconds*max(speed,FR_GAS_PRACTICAL_MIXING_RATE);
    if(!(courant<=.9*float(FR_GAS_EXPLICIT_MAX_STAGES))) {if(rank==0u) atomicAdd(stats[FR_GAS_IMPLICIT_LINE_COUNT_WORD],1u);return;}
    uint stages=uint(ceil(courant/.9));
    stages=max(stages,1u);float peak=0.;
    for(uint cell=rank;cell<length;cell+=FR_GAS_LOCAL_SIZE) {
        uint atlas=gas_atlas(transport_point(line,cell));GasFlux value=atlas==0xffffffffu?gas_empty_flux():transport_state(transport_axis_input(atlas));
        line_species[cell]=value.species;line_dynamics[cell]=value.dynamics;line_visual[cell]=vec4(value.pigment,0);
        peak=max(peak,max(value.species.z,value.dynamics.x));
    }
    peak=line_maximum(peak);
    float scale=peak<=3.402823466e38/(float(length+1u)*4294967296.*16.)?4294967296.:1.;
    if(rank==0u) {
        transport_work[line*length].mass.w=-2.;atomicAdd(stats[FR_GAS_EXPLICIT_LINE_COUNT_WORD],1u);
        atomicAdd(stats[FR_GAS_EXPLICIT_STAGE_COUNT_WORD],stages);
    }
    for(uint stage=0u;stage<stages;stage++) {
        for(uint cell=rank;cell<length;cell+=FR_GAS_LOCAL_SIZE) {
            if(stage!=0u) {
                uint atlas=gas_atlas(transport_point(line,cell));GasFlux value=atlas==0xffffffffu?gas_empty_flux():transport_state(gas_provisional(atlas));
                line_species[cell]=value.species;line_dynamics[cell]=value.dynamics;line_visual[cell]=vec4(value.pigment,0);
            }
            line_species[cell].z*=scale;line_dynamics[cell].x*=scale;line_visual[cell].rgb*=scale;
        }
        barrier();
        for(uint cell=rank;cell<length;cell+=FR_GAS_LOCAL_SIZE) {
            ivec2 p=transport_point(line,cell),step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);uint atlas=gas_atlas(p);
            if(atlas==0xffffffffu) continue;
            GasFlux incoming=line_face_flux(p-step,seconds/float(stages)),outgoing=line_face_flux(p,seconds/float(stages));
            Gas previous=stage==0u?transport_axis_input(atlas):gas_provisional(atlas);
            gas_write_provisional(atlas,transport_update_state(previous,incoming,outgoing,scale));
        }
        memoryBarrierBuffer();barrier();
    }
}

#endif
