#ifndef FR_FLAME_GAS_GLSL
#define FR_FLAME_GAS_GLSL
float gas_mass(Gas gas) {return dot(gas.species,vec4(1))+gas.dynamics.x;}
float gas_capacity(Gas gas) {
    Words model=Words(params.model_address);
    return dot(gas.species,vec4(uintBitsToFloat(model.words[5]),uintBitsToFloat(model.words[6]),
        uintBitsToFloat(model.words[7]),uintBitsToFloat(model.words[8])))+gas.dynamics.x*uintBitsToFloat(model.words[9]);
}
float gas_internal(Gas gas) {
    float mass=gas_mass(gas);
    float kinetic=mass>0.?.5*uintBitsToFloat(Words(params.model_address).words[10])*dot(gas.dynamics.zw,gas.dynamics.zw)/mass:0.;
    return max(0.,gas.dynamics.y+gas.visual.w+gas.geometry.z-kinetic);
}
float flame_emissivity(Gas gas) {
    float density=max(gas.dynamics.x,0.)/max(gas.geometry.x,.000001);
    return 1.-exp(-density/float(FR_COMBUSTION_SOOT_OPTICAL_DENSITY));
}
float carbon_activity(Gas gas) {
    float mass=gas_mass(gas),capacity=gas_capacity(gas);
    if(gas.dynamics.x<=0.||mass<=0.||capacity<=0.||gas.geometry.y<=0.) return 0.;
    float temperature=gas_internal(gas)/capacity,oxygen=gas.species.x/mass;
    float thermal=clamp((temperature-float(FR_COMBUSTION_AIRBORNE_IGNITION_K))/
        float(FR_COMBUSTION_AIRBORNE_FULL_RATE_K-FR_COMBUSTION_AIRBORNE_IGNITION_K),0.,1.);
    float supply=max(oxygen,0.)/float(FR_COMBUSTION_AMBIENT_OXYGEN);
    return float(FR_COMBUSTION_AIRBORNE_RATE)*thermal*supply;
}
float oxidize_carbon(inout Gas gas) {
    float activity=carbon_activity(gas);if(activity<=0.) return 0.;
    float extent=min(gas.dynamics.x*(1.-exp(-activity*uintBitsToFloat(params.seconds))),
        gas.species.x/float(FR_COMBUSTION_AIRBORNE_OXYGEN));
    float oxygen=min(gas.species.x,extent*float(FR_COMBUSTION_AIRBORNE_OXYGEN));
    gas.dynamics.x-=extent;gas.species.x-=oxygen;gas.species.z+=extent+oxygen;
    gas.dynamics.y+=extent*float(FR_COMBUSTION_AIRBORNE_ENERGY);
    return extent*float(FR_COMBUSTION_AIRBORNE_ENERGY);
}
#endif
