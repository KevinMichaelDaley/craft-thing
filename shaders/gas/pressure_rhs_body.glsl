#include "acoustic.glsl"
#ifdef FR_GAS_FUSED_METADATA_RHS
#include "transport_metadata_body.glsl"
vec4 gas_rhs_neighbor(ivec2 p) {
    Gas transported=gas_practical_state(gas_provisional(gas_atlas(p)));bool valid=false;Primitive moved=gas_primitive(transported,valid);
    float work=0.,impulse=0.;Gas value=valid?gas_buoyancy_cached(transported,moved,uintBitsToFloat(params.seconds),work,impulse):transported;
    return vec4(value.dynamics.zw,work,gas_source(p).geometry.x);
}
#endif
Primitive gas_rhs_stage(ivec2 p,uint index) {
#ifdef FR_GAS_FUSED_METADATA_RHS
    if(gas_advected(index)) {
        Primitive value=gas_cached(p);Gas transported=gas_practical_state(gas_provisional(gas_atlas(p)));
        vec2 head=gas_large_pressure_state(transported,value.hydro.w);value.hydro.x=head.x;value.metric.x=head.y;return value;
    }
#endif
    return gas_pressure_stage(p,index);
}
layout(set=0,binding=24,std430) restrict buffer RowBaseline {vec4 row_baseline[];};
void main() {
    uint index=(params.bank&256u)!=0u?gas_sparse_index(2u):gas_index();if(index>=params.width*params.height) return;
    ivec2 p=ivec2(int(index%params.width),int(index/params.width));uint atlas=gas_atlas(p);
    bool advected=gas_advected(index);
#ifdef FR_GAS_FUSED_METADATA_RHS
    if(advected) gas_transport_metadata(index);
#endif
    if(!advected) {
        Primitive own=gas_cached(p);
        acoustic[index]=vec4(own.metric.x,own.hydro.w,own.metric.y,own.metric.y);
        provisional[index]=vec4(own.hydro.yz*own.metric.z,0.,own.metric.w);
        correction_data[index]=vec4(0);force[index]=vec2(0);
    }
    float energy_delta=correction_data[index].w;
    pressure[index]=vec4(0);stencil[index]=vec4(0);correction[index]=0.;correction_data[index]=vec4(0);
    if(atlas==0xffffffffu) return;
    float volume=provisional[index].w;
    if(volume<=0.) return;
    float capacity=acoustic[index].z,old_capacity=acoustic[index].w;
    float internal=old_capacity*acoustic[index].y;
    float rhs=energy_delta+
        (old_capacity>0.?internal*(1.-capacity/old_capacity):internal);
    float dt=uintBitsToFloat(params.seconds),diagonal=capacity;vec4 coefficients=vec4(0);
    bool large_energy=(params.bank&FR_GAS_LARGE_ENERGY_BIT)!=0u;
    Primitive own_stage=large_energy?gas_rhs_stage(p,index):gas_empty_primitive();
    bool rebuild=advected||(params.bank&4u)!=0u;
    if(!rebuild) {coefficients=matrix[index]*(dt*dt);for(uint d=0u;d<4u;d++) diagonal+=coefficients[d];}
    vec4 normalized=vec4(0);
    if(rebuild) for(uint d=0u;d<4u;d++) {
        ivec2 q=p+gas_direction(d);Primitive other=gas_cached(q);
        if(other.metric.w<=0.) continue;
        uint owner=(d&1u)==0u?atlas:gas_atlas(q);
        if(owner!=0xffffffffu&&(gates[owner]&(1u<<(d/2u)))!=0u) continue;
        uint j=uint(q.y)*params.width+uint(q.x);bool moved=gas_advected(j);
#ifdef FR_GAS_FUSED_METADATA_RHS
        vec4 neighbor=moved?gas_rhs_neighbor(q):vec4(other.hydro.yz*other.metric.z,0.,other.metric.w);
#else
        vec4 neighbor=moved?provisional[j]:vec4(other.hydro.yz*other.metric.z,0.,other.metric.w);
#endif
        float area=min(volume,neighbor.w),enthalpy=.5*(acoustic[index].x+other.metric.x);
        if(large_energy) {
            Primitive other_stage=gas_rhs_stage(q,j);
            precise float density=own_stage.hydro.x+other_stage.hydro.x;
            precise float energy=own_stage.hydro.x*own_stage.metric.x+other_stage.hydro.x*other_stage.metric.x;
            enthalpy=density>0.?energy/density:0.;
            if((params.bank&FR_GAS_PRACTICAL_BIT)!=0u&&(own_stage.hydro.x<=0.||other_stage.hydro.x<=0.)) enthalpy=0.;
        }
        if(!large_energy&&(params.bank&FR_GAS_PRACTICAL_BIT)!=0u&&(gas_cached(p).hydro.x<=0.||other.hydro.x<=0.)) enthalpy=0.;
        float coefficient=(dt*dt)*(area*enthalpy);
        normalized[d]=area*enthalpy;
        float flux=.5*(provisional[index][d/2u]/volume+neighbor[d/2u]/neighbor.w)*area;
        rhs+=((d&1u)==0u?-1.:1.)*dt*enthalpy*flux+coefficient*(other.hydro.w-acoustic[index].y);
        coefficients[d]=coefficient;diagonal+=coefficient;
    }
    if(rebuild) matrix[index]=normalized;
    pressure[index]=vec4(0,rhs,diagonal,max(abs(internal),1e-20));stencil[index]=coefficients;
    acoustic[index].w=capacity;
    acoustic[index].z=0.;
    float relaxation=gas_pressure_relaxation(coefficients,diagonal);
    correction_data[index]=vec4(rhs,diagonal>0.?1./diagonal:0.,relaxation,0);
    if((params.bank&FR_GAS_COMPARE_ROW_SETUP_BIT)!=0u) {
        row_baseline[index*2u]=vec4(diagonal,pressure[index].w,correction_data[index].yz);
        row_baseline[index*2u+1u]=coefficients;
    }
}
