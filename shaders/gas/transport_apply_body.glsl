#ifndef FR_GAS_TRANSPORT_APPLY_BODY_GLSL
#define FR_GAS_TRANSPORT_APPLY_BODY_GLSL
float transport_balance_stock(float stock,float incoming,float outgoing) {
    vec2 received=gas_two_sum(stock,incoming);
    vec2 sent=gas_two_sum(received.x,-outgoing);
    precise float residual=received.y+sent.y;
    precise float balance=sent.x+residual;
    return balance;
}
Gas transport_update_state(Gas source,GasFlux incoming,GasFlux outgoing,float trace_scale) {
    float energy_in=incoming.dynamics.y,energy_out=outgoing.dynamics.y;
    bool practical=gas_practical_policy();float old_kinetic=practical?gas_kinetic_energy(source):0.;
    if(practical) {GasFlux remove=gas_empty_flux();remove.dynamics.y=-old_kinetic;source=gas_add_flux(source,remove,1.);}
    vec2 carrier=vec2(
        transport_balance_stock(source.species.z*trace_scale,incoming.species.z,outgoing.species.z),
        transport_balance_stock(source.dynamics.x*trace_scale,incoming.dynamics.x,outgoing.dynamics.x));
    vec3 pigment;
    for(uint k=0u;k<3u;k++)
        pigment[k]=transport_balance_stock(source.visual[k]*trace_scale,incoming.pigment[k],outgoing.pigment[k]);
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
