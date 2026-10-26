#ifndef FR_GAS_TRANSPORT_APPLY_BODY_GLSL
#define FR_GAS_TRANSPORT_APPLY_BODY_GLSL
Gas transport_update_state(Gas source,GasFlux incoming,GasFlux outgoing,float trace_scale) {
    float energy_in=incoming.dynamics.y,energy_out=outgoing.dynamics.y;
    bool practical=(params.bank&FR_GAS_PRACTICAL_BIT)!=0u;float old_kinetic=practical?gas_kinetic_energy(source):0.;
    if(practical) {GasFlux remove=gas_empty_flux();remove.dynamics.y=-old_kinetic;source=gas_add_flux(source,remove,1.);}
    precise vec2 carrier=vec2(source.species.z,source.dynamics.x)*trace_scale+vec2(incoming.species.z,incoming.dynamics.x)-vec2(outgoing.species.z,outgoing.dynamics.x);
    precise vec3 pigment=source.visual.rgb*trace_scale+incoming.pigment-outgoing.pigment;
    GasFlux net=GasFlux(incoming.species-outgoing.species,incoming.dynamics-outgoing.dynamics,vec3(0));net.dynamics.y=0.;net.species.z=0.;net.dynamics.x=0.;
    Gas value=gas_add_flux(source,net,1.);value.species.z=carrier.x/trace_scale;value.dynamics.x=carrier.y/trace_scale;value.visual.rgb=pigment/trace_scale;
    GasFlux energy=gas_empty_flux();
    energy.dynamics.y=energy_in;value=gas_add_flux(value,energy,1.);
    energy.dynamics.y=-energy_out;value=gas_add_flux(value,energy,1.);
    if(practical) {float kinetic=gas_kinetic_energy(value);energy.dynamics.y=kinetic;value=gas_add_flux(value,energy,1.);value.geometry.w+=kinetic-old_kinetic;}
    return value;
}
void transport_apply_cell(ivec2 p) {
    uint atlas=gas_atlas(p);if(atlas==0xffffffffu||transport_work[transport_line_start(p)].mass.w<0.) return;
    ivec2 step=transport_axis()==0u?ivec2(1,0):ivec2(0,1);
    GasFlux incoming=transport_face_flux(p-step),outgoing=transport_face_flux(p);
    gas_write_provisional(atlas,transport_update_state(transport_axis_input(atlas),incoming,outgoing,transport_work[transport_position(p)].mass.w));
}
#endif
