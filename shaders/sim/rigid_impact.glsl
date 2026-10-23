/* Contact work is accumulated in physical units. Stress uses longitudinal wave
 * impedance rho*c, so the trigger is material-derived rather than drop height. */
void impact_capture(uint slot,uint other,uint material,uint other_material,vec2 normal,float incoming,float impulse,float inverse_effective_mass,float restitution) {
    uint base=structural_base();
    if(DC_SOLVER_DATA(base+15u)==0u||slot>=BODY_CAPACITY||incoming>=0||impulse<=0) return;
    Body b=DC_BODY(slot);
    if(b.enabled==0u||b.inverse_mass==0||(!pixel_body(b)&&b.material>1u)||inverse_effective_mass<=0) return;
    uint g=structural_graph(),profile=g+29000u;
    if(pixel_body(b)) profile+=(material==2u||material==4u?1u:material==3u?2u:0u)*12u;
    float density=pixel_body(b)?(material==2u?DC_DENSITY_WOOD:material==4u?DC_DENSITY_DENSE_WOOD:
        material==3u?DC_DENSITY_MORTAR:material==5u?DC_DENSITY_LEAVES:DC_DENSITY_STONE):uintBitsToFloat(DC_SOLVER_DATA(profile+1u));
    float impedance=sqrt(uintBitsToFloat(DC_SOLVER_DATA(profile))*density);
    if(-incoming*60.0*impedance<=(material==5u?500.0:uintBitsToFloat(DC_SOLVER_DATA(profile+4u)))) return;
    uint p=body_impact_pending(slot);
    float retained=1.0-clamp(restitution,0.0,1.0);
    float share=other<BODY_CAPACITY&&DC_BODY(other).inverse_mass>0&&
        (pixel_body(DC_BODY(other))?other_material!=0u:DC_BODY(other).material<=1u)?.5:1.0;
    float work=.5*impulse*(-incoming)*3600.0*retained*share;
    float available=.5*incoming*incoming/inverse_effective_mass*3600.0*retained*(2.0-retained)*share;
    uint units=uint(min(work/16.0,1000000.0));
    atomicMax(DC_SOLVER_DATA(p),units);
    atomicMax(DC_SOLVER_DATA(p+1u),uint(min(available/16.0,1000000.0)));
    vec2 local=body_rotate(normal,-b.angle);
    atomicMax(DC_SOLVER_DATA(p+2u),abs(local.y)>=abs(local.x)?2u:1u);
    if(material==5u) atomicOr(DC_SOLVER_DATA(p+2u),256u);
}
