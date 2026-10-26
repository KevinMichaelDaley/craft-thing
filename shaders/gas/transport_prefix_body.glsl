#ifndef FR_GAS_FUSED_AXIS
#extension GL_GOOGLE_include_directive : require
#extension GL_KHR_shader_subgroup_shuffle_relative : require
#endif
#include "transport_implicit.glsl"
#include "transport_apply_body.glsl"
#ifdef FR_GAS_REUSE_LINE_SHARED
#define GROUP_FACTOR(i) line_species[i].xy
#define GROUP_SPECIES(i) line_species[64u+(i)]
#define GROUP_DYNAMICS(i) line_species[128u+(i)]
#define GROUP_VISUAL(i) line_species[192u+(i)]
#define GROUP_MOVING(i) line_species[256u+(i)].x
#define GROUP_MOVING_TYPE float
#else
shared vec2 group_factor[64];
shared vec4 group_species[64],group_dynamics[64],group_visual[64];
shared uint group_moving[64];
#define GROUP_FACTOR(i) group_factor[i]
#define GROUP_SPECIES(i) group_species[i]
#define GROUP_DYNAMICS(i) group_dynamics[i]
#define GROUP_VISUAL(i) group_visual[i]
#define GROUP_MOVING(i) group_moving[i]
#define GROUP_MOVING_TYPE uint
#endif
GasFlux shuffle_up(GasFlux value,uint offset) {
    return GasFlux(subgroupShuffleUp(value.species,offset),subgroupShuffleUp(value.dynamics,offset),subgroupShuffleUp(value.pigment,offset));
}
void cell_map(ivec2 p,out vec2 factor,out GasFlux value,out bool moving) {
    TransportCell source=transport_work[transport_position(p)];bool backward=transport_direction()<0;
    vec4 mass=source.mass;
#ifdef FR_GAS_COMPACT_WORK
    vec4 energy=mass;moving=source.visual.w!=0.;
#else
    vec4 energy=source.energy;moving=energy.w!=0.;
#endif
    factor=backward?vec2(mass.z,energy.z):vec2(mass.x,energy.x);
    value=GasFlux(source.species,source.dynamics,source.visual.rgb);
    value=transport_scale(value,backward?vec2(1):vec2(mass.y,energy.y));
}
#ifndef FR_GAS_PREFIX_ENTRY
#define FR_GAS_PREFIX_ENTRY main
#endif
void FR_GAS_PREFIX_ENTRY() {
    uint lane=gl_SubgroupInvocationID,rank=gl_SubgroupID*gl_SubgroupSize+lane;
    uint length=transport_axis()==0u?params.width:params.height,span=(length+FR_GAS_LOCAL_SIZE-1u)/FR_GAS_LOCAL_SIZE;
    uint begin=rank*span,end=min(begin+span,length),line=transport_line();
    if(transport_work[line*length].mass.w<0.) return;
    vec2 factor=vec2(1);GasFlux value=gas_empty_flux();bool moving=false;
    for(uint cell=begin;cell<end;cell++) {
        vec2 own_factor;GasFlux own_value;bool own_moving;cell_map(transport_point(line,cell),own_factor,own_value,own_moving);
        moving=moving||own_moving;value=transport_add(own_value,transport_scale(value,own_factor));factor*=own_factor;
    }
    uint moving_mask=subgroupOr(moving?1u:0u);
    if(lane+1u==gl_SubgroupSize) GROUP_MOVING(gl_SubgroupID)=GROUP_MOVING_TYPE(moving_mask);
    barrier();bool any_moving=false;
    for(uint group=0;group<gl_NumSubgroups;group++) any_moving=any_moving||GROUP_MOVING(group)!=GROUP_MOVING_TYPE(0);
    if(!any_moving) {
        for(uint cell=begin;cell<end;cell++) {
            uint index=transport_position(transport_point(line,cell));
            transport_work[index].species=vec4(0);transport_work[index].dynamics=vec4(0);transport_work[index].visual=vec4(0);
        }
        return;
    }
    for(uint offset=1u;offset<gl_SubgroupSize;offset*=2u) {
        vec2 previous=subgroupShuffleUp(factor,offset);GasFlux prior=shuffle_up(value,offset);
        if(lane>=offset) {value=transport_add(value,transport_scale(prior,factor));factor*=previous;}
    }
    if(lane+1u==gl_SubgroupSize) {
        GROUP_FACTOR(gl_SubgroupID)=factor;GROUP_SPECIES(gl_SubgroupID)=value.species;
        GROUP_DYNAMICS(gl_SubgroupID)=value.dynamics;GROUP_VISUAL(gl_SubgroupID)=vec4(value.pigment,0);
    }
    vec2 exclusive_factor=subgroupShuffleUp(factor,1u);GasFlux exclusive_value=shuffle_up(value,1u);
    if(lane==0u) {exclusive_factor=vec2(1);exclusive_value=gas_empty_flux();}
    barrier();GasFlux prefix=gas_empty_flux();
    for(uint group=0;group<gl_SubgroupID;group++) {
        GasFlux prior=GasFlux(GROUP_SPECIES(group),GROUP_DYNAMICS(group),GROUP_VISUAL(group).rgb);
        prefix=transport_add(prior,transport_scale(prefix,GROUP_FACTOR(group)));
    }
    prefix=transport_add(exclusive_value,transport_scale(prefix,exclusive_factor));
    for(uint cell=begin;cell<end;cell++) {
        ivec2 p=transport_point(line,cell);vec2 own_factor;GasFlux own_value;bool own_moving;cell_map(p,own_factor,own_value,own_moving);
        prefix=transport_add(own_value,transport_scale(prefix,own_factor));
        uint index=transport_position(p);transport_work[index].species=prefix.species;
        transport_work[index].dynamics=prefix.dynamics;
        transport_work[index].visual=vec4(prefix.pigment,transport_work[index].visual.w);
    }
#ifdef FR_GAS_FUSED_AXIS
    if(transport_pass!=0u) {
#else
    if((params.bank&FR_GAS_FUSED_APPLY_BIT)!=0u) {
#endif
        memoryBarrierBuffer();barrier();
        for(uint cell=begin;cell<end;cell++) transport_apply_cell(transport_point(line,cell));
    }
}
