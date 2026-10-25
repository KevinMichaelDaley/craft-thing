GasFlux gas_physical_flux(Gas density,Primitive p,uint axis) {
    float velocity=p.hydro[axis+1u];
    GasFlux f=GasFlux(density.species*velocity,density.dynamics*velocity,density.visual.rgb*velocity);
    f.dynamics[axis+2u]+=p.hydro.w;f.dynamics.y=(gas_energy(density)+gas_parameter(FR_GAS_KINETIC_WORD)*p.hydro.w)*velocity;
    return f;
}
Gas gas_density(Gas s,float volume) {
    s.species/=volume;s.dynamics/=volume;s.visual/=volume;s.geometry.z/=volume;return s;
}
GasFlux gas_flux_combine(GasFlux a,float x,GasFlux b,float y) {
    return GasFlux(a.species*x+b.species*y,a.dynamics*x+b.dynamics*y,a.pigment*x+b.pigment*y);
}
GasFlux gas_face(ivec2 a,ivec2 b,uint axis) {
    Primitive pl=gas_cached(a),pr=gas_cached(b);float vl=pl.metric.w,vr=pr.metric.w;
    if(vl==0.&&vr==0.) return gas_empty_flux();
    if((vl==0.||pl.hydro[axis+1u]==0.)&&(vr==0.||pr.hydro[axis+1u]==0.)&&
       (vl==0.||vr==0.||pl.hydro.w==pr.hydro.w)) {
        GasFlux rest=gas_empty_flux();
        rest.dynamics[axis+2u]=vl==0.?pr.hydro.w*vr:(vr==0.?pl.hydro.w*vl:pl.hydro.w*min(vl,vr));
        return rest;
    }
    Gas left=gas_source(a),right=gas_source(b);float area=min(vl,vr);
    if(vl==0.) {left=right;left.dynamics[axis+2u]*=-1.;pl=pr;pl.hydro[axis+1u]*=-1.;vl=vr;area=vr;}
    if(vr==0.) {right=left;right.dynamics[axis+2u]*=-1.;pr=pl;pr.hydro[axis+1u]*=-1.;vr=vl;area=vl;}
    Gas ql=gas_density(left,vl),qr=gas_density(right,vr);
    float u=pl.hydro[axis+1u],v=pr.hydro[axis+1u],rho_l=pl.hydro.x,rho_r=pr.hydro.x,p_l=pl.hydro.w,p_r=pr.hydro.w;
    GasFlux fl=gas_physical_flux(ql,pl,axis),fr=gas_physical_flux(qr,pr,axis),result;
    if(u==v&&p_l==p_r) return gas_flux_combine(u>=0.?fl:fr,area,fl,0.);
    float lo=min(u-pl.metric.x,v-pr.metric.x),hi=max(u+pl.metric.x,v+pr.metric.x);
    if(lo>=0.) return gas_flux_combine(fl,area,fr,0.);
    if(hi<=0.) return gas_flux_combine(fr,area,fl,0.);
    if(rho_l>0.&&rho_r>0.) {
        float contact=(p_r-p_l+rho_l*u*(lo-u)-rho_r*v*(hi-v))/(rho_l*(lo-u)-rho_r*(hi-v));
        bool from_left=contact>=0.;Gas q=from_left?ql:qr;Primitive p=from_left?pl:pr;
        float normal=from_left?u:v,wave=from_left?lo:hi,rho=p.hydro.x,pressure=p.hydro.w;
        float factor=normal==contact?1.:(wave-normal)/(wave-contact),density=rho*factor;
        float star_pressure=pressure+rho*(wave-normal)*(contact-normal);
        float energy=normal==contact?gas_energy(q):
            ((wave-normal)*gas_energy(q)-gas_parameter(FR_GAS_KINETIC_WORD)*pressure*normal+
             gas_parameter(FR_GAS_KINETIC_WORD)*star_pressure*contact)/(wave-contact);
        float transverse=p.hydro[2u-axis];
        float kinetic=.5*gas_parameter(FR_GAS_KINETIC_WORD)*density*(contact*contact+transverse*transverse);
        if(gas_finite(contact)&&contact>=lo&&contact<=hi&&factor>0.&&star_pressure>=0.&&gas_finite(energy)&&energy>kinetic) {
            float coefficient=contact*factor;
            result=GasFlux(q.species*coefficient,vec4(q.dynamics.x*coefficient,
                contact*(energy+gas_parameter(FR_GAS_KINETIC_WORD)*star_pressure),0,0),q.visual.rgb*coefficient);
            result.dynamics[axis+2u]=density*contact*contact+star_pressure;
            result.dynamics[3u-axis]=density*contact*transverse;
            return gas_flux_combine(result,area,fl,0.);
        }
    }
    GasFlux jump=GasFlux(qr.species-ql.species,qr.dynamics-ql.dynamics,qr.visual.rgb-ql.visual.rgb);
    jump.dynamics.y=gas_energy(qr)-gas_energy(ql);
    result=gas_flux_combine(fl,hi/(hi-lo),fr,-lo/(hi-lo));result=gas_flux_combine(result,1.,jump,lo*hi/(hi-lo));
    return gas_flux_combine(result,area,fl,0.);
}
