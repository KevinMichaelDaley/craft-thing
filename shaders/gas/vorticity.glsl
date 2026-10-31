#ifndef FR_GAS_VORTICITY_GLSL
#define FR_GAS_VORTICITY_GLSL
#define FR_GAS_VORTICITY_STRENGTH 2.
#define FR_GAS_VORTICITY_STEP_FRACTION .1
#define FR_GAS_SUBGRID_STRENGTH 1.5
/* A local confinement force restores rotation damped by grid advection.
 * Source caches are immutable throughout finish; walls use free-slip velocity.
 * No random wind, flame mask, extra field scan or solver iteration is involved. */
float gas_curl(ivec2 p) {
    Primitive own=gas_cached(p);if(own.metric.w<=0.) return 0.;
    vec2 velocity[4];
    for(uint d=0u;d<4u;d++) {
        uint neighbor;velocity[d]=gas_neighbor(p,d,neighbor)?gas_cached(p+gas_direction(d)).hydro.yz:own.hydro.yz;
    }
    return .5*((velocity[0].y-velocity[1].y)-(velocity[2].x-velocity[3].x));
}
vec2 gas_vorticity_velocity(ivec2 p,float dt,vec2 velocity) {
    float curl=gas_curl(p);if(abs(curl)<.0001) return vec2(0.);
    float magnitude[4];
    for(uint d=0u;d<4u;d++) {
        uint neighbor;magnitude[d]=gas_neighbor(p,d,neighbor)?abs(gas_curl(p+gas_direction(d))):abs(curl);
    }
    vec2 gradient=.5*vec2(magnitude[0]-magnitude[1],magnitude[2]-magnitude[3]);
    float size=length(gradient);if(size<.0001) return vec2(0.);
    vec2 normal=gradient/size,delta=FR_GAS_VORTICITY_STRENGTH*dt*curl*vec2(normal.y,-normal.x);
    float limit=FR_GAS_VORTICITY_STEP_FRACTION*max(length(velocity),1.),change=length(delta);
    return change>limit?delta*(limit/change):delta;
}
/* Curl of a compact cell streamfunction. The backward-difference divergence
 * cancels exactly, and a zero potential on closed faces makes its global
 * momentum sum telescope to zero. Spatial phase is fixed; only transported
 * resolved shear can change the perturbation from one step to the next. */
float gas_subgrid_potential(ivec2 p,float dt) {
    uint at=gas_atlas(p);if(at==0xffffffffu) return 0.;
    Primitive own=gas_cached(p);if(own.metric.w<=0.||own.metric.z<=0.) return 0.;
    for(uint d=0u;d<4u;d++) {uint neighbor;if(!gas_neighbor(p,d,neighbor)) return 0.;}
    float curl=gas_curl(p);if(abs(curl)<.001) return 0.;
    uint phase=uint(p.x)*1664525u+uint(p.y)*1013904223u;
    phase^=phase>>16u;phase*=2246822519u;phase^=phase>>13u;
    float signed_phase=float(phase&65535u)*(2./65535.)-1.;
    return FR_GAS_SUBGRID_STRENGTH*dt*own.metric.z*clamp(curl,-20.,20.)*signed_phase;
}
vec2 gas_subgrid_momentum(ivec2 p,float dt) {
    float own=gas_subgrid_potential(p,dt);
    return vec2(own-gas_subgrid_potential(p-ivec2(0,1),dt),
                gas_subgrid_potential(p-ivec2(1,0),dt)-own);
}
#endif
