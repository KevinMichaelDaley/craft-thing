#ifndef FR_GAS_VORTICITY_GLSL
#define FR_GAS_VORTICITY_GLSL
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
    vec2 normal=gradient/size,delta=1.*dt*curl*vec2(normal.y,-normal.x);
    float limit=.1*max(length(velocity),1.),change=length(delta);
    return change>limit?delta*(limit/change):delta;
}
#endif
