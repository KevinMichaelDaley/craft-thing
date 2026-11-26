#ifndef FR_GAS_TRANSPORT_APPLY_BODY_GLSL
#define FR_GAS_TRANSPORT_APPLY_BODY_GLSL
vec2 transport_balance_stock(vec2 stock,float incoming,float outgoing) {
    vec2 received=gas_two_sum(stock.x,incoming);
    vec2 sent=gas_two_sum(received.x,-outgoing);
    precise float residual=(received.y+sent.y)+stock.y;
    return gas_two_sum(sent.x,residual);
}
Gas transport_update_state(Gas source,GasFlux incoming,GasFlux outgoing,float trace_scale) {
    float energy_in=incoming.dynamics.y,energy_out=outgoing.dynamics.y;
    bool practical=gas_practical_policy();float old_kinetic=practical?gas_kinetic_energy(source):0.;
    if(practical) {GasFlux remove=gas_empty_flux();remove.dynamics.y=-old_kinetic;source=gas_add_flux(source,remove,1.);}
    vec2 exhaust=transport_balance_stock(gas_exhaust_stock(source)*trace_scale,incoming.species.z,outgoing.species.z);
    vec2 soot=transport_balance_stock(gas_soot_stock(source)*trace_scale,incoming.dynamics.x,outgoing.dynamics.x);
    vec3 pigment=vec3(
        transport_balance_stock(vec2(source.visual.r*trace_scale,0),incoming.pigment.r,outgoing.pigment.r).x,
        transport_balance_stock(vec2(source.visual.g*trace_scale,0),incoming.pigment.g,outgoing.pigment.g).x,
        transport_balance_stock(vec2(source.visual.b*trace_scale,0),incoming.pigment.b,outgoing.pigment.b).x);
    GasFlux net=GasFlux(incoming.species-outgoing.species,incoming.dynamics-outgoing.dynamics,vec3(0));net.dynamics.y=0.;net.species.z=0.;net.dynamics.x=0.;
    Gas value=gas_set_carriers(gas_add_flux(source,net,1.),exhaust/trace_scale,soot/trace_scale);
    value.visual.rgb=pigment/trace_scale;
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
