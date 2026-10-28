#ifndef FR_GAS_METADATA_PREDICTOR_GLSL
#define FR_GAS_METADATA_PREDICTOR_GLSL
/* Preserve the fused neighbor forecast's evaluation order, distinct from owning EOS. */
vec4 gas_rhs_neighbor(Primitive source,Gas transported) {
    float mass=gas_mass(transported);if(mass<=0.) return vec4(transported.dynamics.zw,0.,source.metric.w);
    if(mass<1e-20) {
        bool valid=false;Primitive moved=gas_primitive(transported,valid);if(!valid) return vec4(0,0,0,source.metric.w);
        float work=0.,impulse=0.;Gas value=gas_buoyancy_cached(transported,moved,uintBitsToFloat(params.seconds),work,impulse);
        return vec4(value.dynamics.zw,work,source.metric.w);
    }
    float capacity=dot(transported.species,vec4(gas_parameter(5u),gas_parameter(6u),gas_parameter(7u),gas_parameter(8u)))+transported.dynamics.x*gas_parameter(9u);
    float moles=dot(transported.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+transported.dynamics.x*gas_parameter(4u);
    float temperature=gas_thermal_energy(transported)/capacity;
    float pressure=moles*temperature/(transported.geometry.x*gas_parameter(FR_GAS_KINETIC_WORD));
    Primitive moved=gas_empty_primitive();moved.hydro.x=mass/transported.geometry.x;moved.hydro.w=pressure;
    float work=0.,impulse=0.;Gas value=gas_buoyancy_cached(transported,moved,uintBitsToFloat(params.seconds),work,impulse);
    return vec4(value.dynamics.zw,work,source.metric.w);
}
#endif
