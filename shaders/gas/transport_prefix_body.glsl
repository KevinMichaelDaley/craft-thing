#extension GL_GOOGLE_include_directive : require
#extension GL_KHR_shader_subgroup_shuffle_relative : require
#include "transport_implicit.glsl"
#include "transport_apply_body.glsl"
shared vec2 group_factor[64];
shared vec4 group_species[64],group_dynamics[64],group_visual[64];
shared uint group_moving[64];
GasFlux shuffle_up(GasFlux value,uint offset) {
    return GasFlux(subgroupShuffleUp(value.species,offset),subgroupShuffleUp(value.dynamics,offset),subgroupShuffleUp(value.pigment,offset));
}
void cell_map(ivec2 p,out vec2 factor,out GasFlux value,out bool moving) {
    TransportCell source=transport_work[transport_position(p)];bool backward=transport_direction()<0;
    vec4 mass=source.mass,energy=source.energy;
    factor=backward?vec2(mass.z,energy.z):vec2(mass.x,energy.x);
    moving=energy.w!=0.;value=GasFlux(source.species,source.dynamics,source.visual.rgb);
    value=transport_scale(value,backward?vec2(1):vec2(mass.y,energy.y));
}
void main() {
    uint lane=gl_SubgroupInvocationID,rank=gl_SubgroupID*gl_SubgroupSize+lane;
    uint length=transport_axis()==0u?params.width:params.height,span=(length+FR_GAS_LOCAL_SIZE-1u)/FR_GAS_LOCAL_SIZE;
    uint begin=rank*span,end=min(begin+span,length),line=gl_WorkGroupID.x;
    if(transport_work[line*length].mass.w<0.) return;
    vec2 factor=vec2(1);GasFlux value=gas_empty_flux();bool moving=false;
    for(uint cell=begin;cell<end;cell++) {
        vec2 own_factor;GasFlux own_value;bool own_moving;cell_map(transport_point(line,cell),own_factor,own_value,own_moving);
        moving=moving||own_moving;value=transport_add(own_value,transport_scale(value,own_factor));factor*=own_factor;
    }
    uint moving_mask=subgroupOr(moving?1u:0u);
    if(lane+1u==gl_SubgroupSize) group_moving[gl_SubgroupID]=moving_mask;
    barrier();bool any_moving=false;
    for(uint group=0;group<gl_NumSubgroups;group++) any_moving=any_moving||group_moving[group]!=0u;
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
        group_factor[gl_SubgroupID]=factor;group_species[gl_SubgroupID]=value.species;
        group_dynamics[gl_SubgroupID]=value.dynamics;group_visual[gl_SubgroupID]=vec4(value.pigment,0);
    }
    vec2 exclusive_factor=subgroupShuffleUp(factor,1u);GasFlux exclusive_value=shuffle_up(value,1u);
    if(lane==0u) {exclusive_factor=vec2(1);exclusive_value=gas_empty_flux();}
    barrier();GasFlux prefix=gas_empty_flux();
    for(uint group=0;group<gl_SubgroupID;group++) {
        GasFlux prior=GasFlux(group_species[group],group_dynamics[group],group_visual[group].rgb);
        prefix=transport_add(prior,transport_scale(prefix,group_factor[group]));
    }
    prefix=transport_add(exclusive_value,transport_scale(prefix,exclusive_factor));
    for(uint cell=begin;cell<end;cell++) {
        ivec2 p=transport_point(line,cell);vec2 own_factor;GasFlux own_value;bool own_moving;cell_map(p,own_factor,own_value,own_moving);
        prefix=transport_add(own_value,transport_scale(prefix,own_factor));
        uint index=transport_position(p);transport_work[index].species=prefix.species;
        transport_work[index].dynamics=prefix.dynamics;transport_work[index].visual=vec4(prefix.pigment,0);
    }
    if((params.bank&FR_GAS_FUSED_APPLY_BIT)!=0u) {
        memoryBarrierBuffer();barrier();
        for(uint cell=begin;cell<end;cell++) transport_apply_cell(transport_point(line,cell));
    }
}
