/* Contact work is accumulated in physical units. Stress uses longitudinal wave
 * impedance rho*c, so the trigger is material-derived rather than drop height. */
void impact_capture(uint slot,uint other,vec2 normal,float incoming,float impulse,float inverse_effective_mass,float restitution) {
    uint base=structural_base();
    if(data[base+15u]==0u||slot>=BODY_CAPACITY||incoming>=0||impulse<=0) return;
    Body b=bodies[slot];
    if(b.enabled==0u||b.inverse_mass==0||b.material>1u||inverse_effective_mass<=0) return;
    uint g=structural_graph(),profile=g+29000u;
    float impedance=sqrt(uintBitsToFloat(data[profile])*uintBitsToFloat(data[profile+1u]));
    if(-incoming*60.0*impedance<=uintBitsToFloat(data[profile+4u])) return;
    uint p=g+38000u+slot*4u;
    float retained=1.0-clamp(restitution,0.0,1.0);
    float share=other<BODY_CAPACITY&&bodies[other].inverse_mass>0&&bodies[other].material<=1u?.5:1.0;
    float work=.5*impulse*(-incoming)*3600.0*retained*share;
    float available=.5*incoming*incoming/inverse_effective_mass*3600.0*retained*(2.0-retained)*share;
    uint units=uint(min(work/16.0,1000000.0));
    atomicMax(data[p],units);
    atomicMax(data[p+1u],uint(min(available/16.0,1000000.0)));
    vec2 local=body_rotate(normal,-b.angle);
    atomicMax(data[p+2u],abs(local.y)>=abs(local.x)?2u:1u);
}
