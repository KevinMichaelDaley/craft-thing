#ifndef FR_GAS_MODEL_GLSL
#define FR_GAS_MODEL_GLSL
float gas_parameter(uint word) {return uintBitsToFloat(parameters[word]);}
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
        return Primitive(vec4(0),vec4(0,0,0,volume));
    }
    okay=okay&&volume>0.&&gas_finite(volume);
    if(!okay) return gas_empty_primitive();
    float capacity=dot(s.species,vec4(gas_parameter(5u),gas_parameter(6u),gas_parameter(7u),gas_parameter(8u)))+s.dynamics.x*gas_parameter(9u);
    float moles=dot(s.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+s.dynamics.x*gas_parameter(4u);
    float kinetic=.5*gas_parameter(FR_GAS_KINETIC_WORD)*dot(s.dynamics.zw,s.dynamics.zw)/mass;
    float temperature=(gas_energy(s)-kinetic)/capacity;
    float pressure=moles*temperature/(volume*gas_parameter(FR_GAS_KINETIC_WORD));
    float sound=sqrt((1.+moles/capacity)*pressure*volume/mass);
    okay=okay&&temperature>0.&&gas_finite(temperature)&&gas_finite(pressure)&&gas_finite(sound);
    return Primitive(vec4(mass/volume,s.dynamics.zw/mass,pressure),vec4(sound,temperature,mass,volume));
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
    vec2 first=gas_two_sum(s.dynamics.y,flux.dynamics.y*dt),second=gas_two_sum(s.visual.w,first.y);
    precise float last=s.geometry.z+second.y;
    vec2 middle=gas_two_sum(second.x,last),top=gas_two_sum(first.x,middle.x),tail=gas_two_sum(top.y,middle.y);
    result.dynamics.y=top.x;result.visual.w=tail.x;result.geometry.z=tail.y;
    return result;
}
Gas gas_buoyancy(Gas s,float dt,out float admitted_work,out float admitted_impulse) {
    admitted_work=0.;admitted_impulse=0.;
    float gravity=gas_parameter(FR_GAS_GRAVITY_WORD),mass=gas_mass(s);
    if(gravity==0.||mass==0.) return s;
    bool okay=false;Primitive primitive=gas_primitive(s,okay);if(!okay) return s;
    float fraction=gas_parameter(FR_GAS_OXYGEN_WORD);
    float oxygen=(fraction/gas_parameter(0u))/(fraction/gas_parameter(0u)+(1.-fraction)/gas_parameter(1u));
    float reference=(oxygen*gas_parameter(0u)+(1.-oxygen)*gas_parameter(1u))*gas_parameter(FR_GAS_AMBIENT_WORD);
    float molecular=dot(s.species,vec4(gas_parameter(0u),gas_parameter(1u),gas_parameter(2u),gas_parameter(3u)))+s.dynamics.x*gas_parameter(4u);
    float ratio=molecular*primitive.metric.y/(mass*reference);
    float acceleration=abs(ratio-1.)<=0.00000095367431640625?0.:gravity*(1.-ratio);
    float impulse=mass*acceleration*dt;
    admitted_impulse=impulse;
    float work=gas_parameter(FR_GAS_KINETIC_WORD)*impulse*(s.dynamics.w+.5*impulse)/mass;
    admitted_work=work;
    GasFlux flux=gas_empty_flux();flux.dynamics.y=work;flux.dynamics.w=impulse;
    Gas result=gas_add_flux(s,flux,1.);result.geometry.w+=work;return result;
}
Gas gas_buoyancy(Gas s,float dt,out float work) {float impulse;return gas_buoyancy(s,dt,work,impulse);}
Gas gas_buoyancy(Gas s,float dt) {float work;return gas_buoyancy(s,dt,work);}
#endif
