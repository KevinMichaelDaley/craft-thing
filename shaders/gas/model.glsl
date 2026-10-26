#ifndef FR_GAS_MODEL_GLSL
#define FR_GAS_MODEL_GLSL
float gas_parameter(uint word) {return uintBitsToFloat(parameters[word]);}
bool gas_practical_policy() {
#ifdef FR_GAS_FORCE_PRACTICAL
    return true;
#else
    return (params.bank&FR_GAS_PRACTICAL_BIT)!=0u;
#endif
}
bool gas_implicit_policy() {
#ifdef FR_GAS_FORCE_IMPLICIT
    return true;
#else
    return gas_parameter(FR_GAS_POLICY_WORD)!=0.;
#endif
}
float gas_mass(Gas s) {return dot(s.species,vec4(1))+s.dynamics.x;}
float gas_energy(Gas s) {return s.dynamics.y+s.visual.w+s.geometry.z;}
bool gas_finite(float x) {return !isnan(x)&&!isinf(x);}
Primitive gas_primitive(Gas s,out bool okay) {
    float mass=gas_mass(s),volume=s.geometry.x;
    okay=all(greaterThanEqual(s.species,vec4(0)))&&s.dynamics.x>=0.&&
        all(greaterThanEqual(s.visual.rgb,vec3(0)))&&
        all(lessThanEqual(s.visual.rgb,vec3((s.species.z+s.dynamics.x)*1.000001)))&&
        gas_finite(mass)&&gas_finite(gas_energy(s))&&all(not(isnan(s.dynamics.zw)))&&all(not(isinf(s.dynamics.zw)));
    if(mass==0.) {
        okay=okay&&gas_energy(s)==0.&&all(equal(s.dynamics.zw,vec2(0)));
        return Primitive(vec4(0),vec4(0,0,0,s.geometry.y));
    }
    okay=okay&&volume>0.&&gas_finite(volume);
    if(!okay) return gas_empty_primitive();
    float capacity=dot(s.species,vec4(gas_parameter(5u),gas_parameter(6u),gas_parameter(7u),gas_parameter(8u)))+s.dynamics.x*gas_parameter(9u);
    float moles=dot(s.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+s.dynamics.x*gas_parameter(4u);
    float kinetic=.5*gas_parameter(FR_GAS_KINETIC_WORD)*dot(s.dynamics.zw,s.dynamics.zw)/mass;
    float temperature=(gas_energy(s)-kinetic)/capacity;
    float pressure=moles*temperature/(volume*gas_parameter(FR_GAS_KINETIC_WORD));
    okay=okay&&temperature>0.&&gas_finite(temperature)&&gas_finite(pressure);
    if(gas_implicit_policy()) {
        float enthalpy=(gas_energy(s)+gas_parameter(FR_GAS_KINETIC_WORD)*pressure*volume)/mass;
        float pressure_capacity=moles>0.?gas_parameter(FR_GAS_KINETIC_WORD)*volume*capacity/moles:0.;
        okay=okay&&gas_finite(enthalpy)&&gas_finite(pressure_capacity);
        return Primitive(vec4(mass/volume,s.dynamics.zw/mass,pressure),vec4(enthalpy,pressure_capacity,mass,s.geometry.y));
    }
    float sound=sqrt((1.+moles/capacity)*pressure*volume/mass);
    okay=okay&&temperature>0.&&gas_finite(temperature)&&gas_finite(pressure)&&gas_finite(sound);
    return Primitive(vec4(mass/volume,s.dynamics.zw/mass,pressure),vec4(sound,temperature,mass,s.geometry.y));
}
vec2 gas_two_sum(float a,float b) {
    precise float sum=a+b;
    precise float recovered=sum-a;
    precise float error=(a-(sum-recovered))+(b-recovered);
    return vec2(sum,error);
}
Gas gas_add_flux(Gas s,GasFlux flux,float dt) {
    Gas result=s;
    result.species+=flux.species*dt;
    result.dynamics.x+=flux.dynamics.x*dt;
    result.dynamics.zw+=flux.dynamics.zw*dt;
    result.visual.rgb+=flux.pigment*dt;
#ifdef FR_GAS_FORCE_IMPLICIT
    /* A zero exchange retains the existing compensated energy expansion. */
    if((params.bank&FR_GAS_ZERO_ENERGY_BIT)!=0u&&flux.dynamics.y==0.) return result;
#endif
    vec2 first=gas_two_sum(s.dynamics.y,flux.dynamics.y*dt),second=gas_two_sum(s.visual.w,first.y);
    precise float last=s.geometry.z+second.y;
    vec2 middle=gas_two_sum(second.x,last),top=gas_two_sum(first.x,middle.x),tail=gas_two_sum(top.y,middle.y);
    result.dynamics.y=top.x;result.visual.w=tail.x;result.geometry.z=tail.y;
    return result;
}
float gas_kinetic_energy(Gas value) {
    float mass=gas_mass(value);
    return mass>0.?.5*gas_parameter(FR_GAS_KINETIC_WORD)*dot(value.dynamics.zw,value.dynamics.zw)/mass:0.;
}
float gas_thermal_energy(Gas value) {return gas_energy(value)-gas_kinetic_energy(value);}
Gas gas_practical_momentum(Gas value,float thermal) {
    float mass=gas_mass(value),largest=max(abs(value.dynamics.z),abs(value.dynamics.w));
    if(mass<=0.||any(isnan(value.dynamics.zw))||any(isinf(value.dynamics.zw))) {
        if(mass>0.) atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],FR_GAS_LIMITED_KINETIC);
        value.dynamics.zw=vec2(0);return value;
    }
    if(largest>0.) {
        float cv=dot(value.species,vec4(gas_parameter(5u),gas_parameter(6u),gas_parameter(7u),gas_parameter(8u)))+value.dynamics.x*gas_parameter(9u);
        float molecular=dot(value.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+value.dynamics.x*gas_parameter(4u);
        float gamma=cv>0.?1.+molecular/cv:1.;
        float budget=max(0.,.125*gamma*(gamma-1.)*thermal);vec2 direction=value.dynamics.zw/largest;
        float kinetic=gas_kinetic_energy(value);
        if(budget>0.&&gas_finite(kinetic)&&kinetic<.98*budget) return value;
        float factor=(sqrt(budget)*sqrt(mass)/largest)*sqrt(2./gas_parameter(FR_GAS_KINETIC_WORD))/sqrt(dot(direction,direction));
        if(factor<1.) {value.dynamics.zw*=factor;atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],FR_GAS_LIMITED_KINETIC);}
    }
    return value;
}
Gas gas_practical_state(Gas value) {
    float scale=dot(abs(value.species),vec4(1))+abs(value.dynamics.x);
    if(!gas_finite(scale)) return value;
    float tolerance=max(scale*0.000003814697265625,1.17549435e-38);uint repaired=0u;
    for(uint k=0u;k<4u;k++) if(value.species[k]<0.&&value.species[k]>=-tolerance) {value.species[k]=0.;repaired|=FR_GAS_RECONCILED_ROUNDOFF;}
    if(value.dynamics.x<0.&&value.dynamics.x>=-tolerance) {value.dynamics.x=0.;repaired|=FR_GAS_RECONCILED_ROUNDOFF;}
    float carrier=value.species.z+value.dynamics.x;
    for(uint k=0u;k<3u;k++) {
        if(value.visual[k]<0.&&value.visual[k]>=-tolerance) {value.visual[k]=0.;repaired|=FR_GAS_RECONCILED_ROUNDOFF;}
        if(value.visual[k]>carrier&&value.visual[k]-carrier<=tolerance) {value.visual[k]=carrier;repaired|=FR_GAS_RECONCILED_ROUNDOFF;}
    }
    float mass=gas_mass(value),energy=gas_energy(value);
    if(mass>0.&&gas_finite(energy)) {
        float minimum=mass*0.000001;
        if(energy<minimum&&energy>=-minimum) {
            GasFlux fix=gas_empty_flux();fix.dynamics.y=minimum-energy;value=gas_add_flux(value,fix,1.);energy=gas_energy(value);repaired|=FR_GAS_FLOORED_ENERGY;
        }
        float largest=max(abs(value.dynamics.z),abs(value.dynamics.w));
        if(energy>0.&&largest>0.&&gas_finite(largest)) {
            float kinetic=gas_kinetic_energy(value);
            if(gas_finite(kinetic)&&kinetic<.98*energy) {
                if(repaired!=0u) atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],repaired);
                return value;
            }
            vec2 direction=value.dynamics.zw/largest;
            float factor=(sqrt(energy)*sqrt(mass)/largest)*sqrt(1.98/gas_parameter(FR_GAS_KINETIC_WORD))/sqrt(dot(direction,direction));
            if(factor<1.) {value.dynamics.zw*=factor;repaired|=FR_GAS_LIMITED_KINETIC;}
        }
    } else if(mass==0.&&all(equal(value.species,vec4(0)))&&value.dynamics.x==0.) {
        value.dynamics.zw=vec2(0);
        if(energy!=0.) {
            float tolerance=FR_GAS_MASSLESS_ENERGY_RELATIVE*gas_parameter(FR_GAS_DENSITY_WORD)*gas_parameter(FR_GAS_AMBIENT_WORD)*
                max(gas_parameter(FR_GAS_CV_WORD),gas_parameter(FR_GAS_CV_WORD+1u));
            if(gas_finite(tolerance)&&abs(energy)<=tolerance) {
                value.dynamics.y=0.;value.visual.w=0.;value.geometry.z=0.;repaired|=FR_GAS_RECONCILED_ROUNDOFF;
            }
        }
    }
    if(repaired!=0u) atomicOr(stats[FR_GAS_REJECT_STAGE_WORD],repaired);
    return value;
}
Gas gas_buoyancy_cached(Gas s,Primitive primitive,float dt,out float admitted_work,out float admitted_impulse) {
    admitted_work=0.;admitted_impulse=0.;
    float gravity=gas_parameter(FR_GAS_GRAVITY_WORD),mass=gas_mass(s);
    if(gravity==0.||mass==0.) return s;
    float fraction=gas_parameter(FR_GAS_OXYGEN_WORD);
    float oxygen=(fraction/gas_parameter(0u))/(fraction/gas_parameter(0u)+(1.-fraction)/gas_parameter(1u));
    float reference=(oxygen*gas_parameter(0u)+(1.-oxygen)*gas_parameter(1u))*gas_parameter(FR_GAS_AMBIENT_WORD);
    float molecular=dot(s.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+s.dynamics.x*gas_parameter(4u);
    float ratio=!gas_implicit_policy()?molecular*primitive.metric.y/(mass*reference):
        gas_parameter(FR_GAS_KINETIC_WORD)*primitive.hydro.w/(primitive.hydro.x*reference);
    float acceleration=abs(ratio-1.)<=0.00000095367431640625?0.:gravity*(1.-ratio);
    float impulse=mass*acceleration*dt;
    admitted_impulse=impulse;
    float work=gas_parameter(FR_GAS_KINETIC_WORD)*impulse*(s.dynamics.w+.5*impulse)/mass;
    admitted_work=work;
    GasFlux flux=gas_empty_flux();flux.dynamics.y=work;flux.dynamics.w=impulse;
    Gas result=gas_add_flux(s,flux,1.);result.geometry.w+=work;return result;
}
Gas gas_buoyancy(Gas s,float dt,out float work,out float impulse) {
    bool okay=false;Primitive primitive=gas_primitive(s,okay);
    if(!okay) {work=0.;impulse=0.;return s;}
    return gas_buoyancy_cached(s,primitive,dt,work,impulse);
}
Gas gas_buoyancy(Gas s,float dt,out float work) {float impulse;return gas_buoyancy(s,dt,work,impulse);}
Gas gas_buoyancy(Gas s,float dt) {float work;return gas_buoyancy(s,dt,work);}
#endif
