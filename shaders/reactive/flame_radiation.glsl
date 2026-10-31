#ifndef FR_FLAME_RADIATION_GLSL
#define FR_FLAME_RADIATION_GLSL

bool flame_visible(ivec2 source,ivec2 target) {
    ivec2 displacement=target-source;
    uint steps=2u*uint(max(abs(displacement.x),abs(displacement.y)));
    for(uint step=1u;step<steps;step++) {
        ivec2 cell=ivec2(floor(vec2(source)+vec2(.5)+vec2(displacement)*float(step)/float(steps)));
        if(all(equal(cell,source))||all(equal(cell,target))) continue;
        uint index=uint(cell.y)*params.width+uint(cell.x);
        if(samples[index].y>0.||thermal_occupancy[index]!=0u) return false;
    }
    return true;
}
float flame_target_temperature(uint index) {
    float capacity=samples[index].y;
    if(capacity<=0.) return 0.;
    vec4 metadata=samples[index+params.width*params.height];
    if(floatBitsToUint(metadata.x)==4u) {
        Words cache=Words(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
        uint handle=floatBitsToUint(metadata.y);
        if(cache.words[handle*12u+8u]==floatBitsToUint(metadata.w)) {
            float skin=uintBitsToFloat(cache.words[handle*12u+9u]);
            if(skin>0.) return skin;
        }
    }
    return samples[index].x/capacity;
}
float flame_pair_flux(uint source,uint target,float source_temperature,float source_capacity,float emissivity) {
    if(samples[target].y<=0.) return 0.;
    ivec2 a=ivec2(source%params.width,source/params.width);
    ivec2 b=ivec2(target%params.width,target/params.width),d=b-a;
    float distance_squared=dot(vec2(d),vec2(d));
    if(distance_squared<=0.||distance_squared>float(FR_THERMAL_RADIATION_RADIUS*FR_THERMAL_RADIATION_RADIUS)||
       !flame_visible(a,b)) return 0.;
    float target_temperature=flame_target_temperature(target);
    float source_square=source_temperature*source_temperature,target_square=target_temperature*target_temperature;
    float projection=float(max(abs(d.x),abs(d.y)))*inversesqrt(distance_squared);
    float coefficient=projection*float(FR_THERMAL_STEFAN_BOLTZMANN*FR_THERMAL_FACE_AREA*FR_THERMAL_SURFACE_EMISSIVITY)*
        emissivity/(3.14159265*distance_squared);
    float requested=coefficient*(source_square*source_square-target_square*target_square)*uintBitsToFloat(params.seconds);
    float capacity=samples[target].y;
    float stable=source_capacity*capacity/(source_capacity+capacity)*abs(source_temperature-target_temperature)/8.;
    return sign(requested)*min(abs(requested),stable);
}
void flame_atomic_heat(uint index,float heat) {
    if(heat==0.) return;
    uint old=atomicAdd(radiation_heat[index],0u);
    for(;;) {
        uint next=floatBitsToUint(uintBitsToFloat(old)+heat);
        uint observed=atomicCompSwap(radiation_heat[index],old,next);
        if(observed==old) break;
        old=observed;
    }
}
void flame_emit(uint source,uint at) {
    if(!gas_cell(at)) return;
    Gas gas=local_gas(at);
    float capacity=gas_capacity(gas),internal=gas_internal(gas),emissivity=flame_emissivity(gas);
    if(gas.geometry.y<=0.||capacity<=0.||internal<=0.||emissivity<=0.) return;
    float temperature=internal/capacity;
    ivec2 p=ivec2(source%params.width,source/params.width);
    float outgoing=0.,incoming=0.,hottest=temperature;
    for(int dy=-int(FR_THERMAL_RADIATION_RADIUS);dy<=int(FR_THERMAL_RADIATION_RADIUS);dy++)
    for(int dx=-int(FR_THERMAL_RADIATION_RADIUS);dx<=int(FR_THERMAL_RADIATION_RADIUS);dx++) {
        ivec2 q=p+ivec2(dx,dy);
        if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(params.width,params.height)))||
           dx*dx+dy*dy>int(FR_THERMAL_RADIATION_RADIUS*FR_THERMAL_RADIATION_RADIUS)) continue;
        uint target=uint(q.y)*params.width+uint(q.x);
        float heat=flame_pair_flux(source,target,temperature,capacity,emissivity);
        outgoing+=max(heat,0.);incoming+=max(-heat,0.);
        if(heat<0.) hottest=max(hottest,flame_target_temperature(target));
    }
    if(outgoing==0.&&incoming==0.) return;
    float out_scale=outgoing>0.?min(1.,internal/(4.*outgoing)):1.;
    float in_scale=incoming>0.?min(1.,capacity*(hottest-temperature)/(4.*incoming)):1.;
    float net=0.;
    for(int dy=-int(FR_THERMAL_RADIATION_RADIUS);dy<=int(FR_THERMAL_RADIATION_RADIUS);dy++)
    for(int dx=-int(FR_THERMAL_RADIATION_RADIUS);dx<=int(FR_THERMAL_RADIATION_RADIUS);dx++) {
        ivec2 q=p+ivec2(dx,dy);
        if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(params.width,params.height)))||
           dx*dx+dy*dy>int(FR_THERMAL_RADIATION_RADIUS*FR_THERMAL_RADIATION_RADIUS)) continue;
        uint target=uint(q.y)*params.width+uint(q.x);
        float requested=flame_pair_flux(source,target,temperature,capacity,emissivity);
        float heat=requested*(requested>=0.?out_scale:in_scale);
        flame_atomic_heat(target,heat);net+=heat;
    }
    bool alternate=params.sparse!=0u&&(Words(params.gates_address).words[at]&8u)!=0u;
    GasField field=GasField(alternate?params.alternate_address:params.world_address);
    gas.dynamics.y-=net;field.values[at]=gas;
}
void flame_apply(uint index,uint at) {
    float heat=uintBitsToFloat(radiation_heat[index]);
    if(heat==0.||samples[index].y<=0.) return;
    uint kind=floatBitsToUint(samples[index+params.width*params.height].x);
    if(kind==4u) fr_body_deposit(index,heat);
    else if(kind==1u) {uvec2 address;if(solid_address(at,address)) add_energy(Words(address),heat);}
    else if(kind==2u&&at!=0xffffffffu) water_heat_words[at]=floatBitsToUint(uintBitsToFloat(water_heat_words[at])+heat);
}

#endif
