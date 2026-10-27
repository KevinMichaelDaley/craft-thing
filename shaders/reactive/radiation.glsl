#include "../../include/dungeoncraft/thermal_surface.h"
float radiant_fourth(float temperature_K) {float square=temperature_K*temperature_K;return square*square;}
float radiant_temperature(uint index) {return temperature(samples[index],index);}
float radiant_available(uint index) {
    vec4 meta=samples[index+params.width*params.height];
    if(floatBitsToUint(meta.x)!=4u) return radiant_temperature(index)*samples[index].y;
    ParcelSamples cache=ParcelSamples(uvec2(root[FR_ARENA_THERMAL_SCRATCH],root[FR_ARENA_THERMAL_SCRATCH+1u]));
    vec4 parcel=cache.values[floatBitsToUint(meta.y)*3u];
    return parcel.y>0.?max(0.,parcel.x)*samples[index].y/parcel.y:0.;
}
float radiant_coefficient(uint first,uint second) {
    ivec2 a=ivec2(first%params.width,first/params.width),b=ivec2(second%params.width,second/params.width);
    float distance=float(abs(a.x-b.x)+abs(a.y-b.y));
    return float(FR_THERMAL_STEFAN_BOLTZMANN*FR_THERMAL_FACE_AREA*FR_THERMAL_SURFACE_EMISSIVITY)/(3.14159265*distance*distance);
}
vec2 radiant_ray(uint index,ivec2 direction) {
    if(samples[index].y<=0.) return vec2(0);
    ivec2 p=ivec2(index%params.width,index/params.width);
    for(uint distance=1u;distance<=FR_THERMAL_RADIATION_RADIUS;distance++) {
        ivec2 q=p+direction*int(distance);if(any(lessThan(q,ivec2(0)))||any(greaterThanEqual(q,ivec2(params.width,params.height)))) break;
        uint other=uint(q.y)*params.width+uint(q.x);
        if(samples[other].y<=0.) {if(thermal_occupancy[other]!=0u) break;continue;}
        if(distance==1u&&thermal_occupancy[index]==thermal_occupancy[other]) break;
        float a=radiant_temperature(index),b=radiant_temperature(other);
        float requested=radiant_coefficient(index,other)*(radiant_fourth(a)-radiant_fourth(b))*uintBitsToFloat(params.seconds);
        float budget=min(samples[index].y,samples[other].y)*abs(a-b)/16.;
        budget=min(budget,(requested>=0.?radiant_available(index):radiant_available(other))/16.);
        return vec2(sign(requested)*min(abs(requested),budget),uintBitsToFloat(other+1u));
    }
    return vec2(0);
}
void radiant_add_pair(uint first,uint second,float heat,inout float outgoing,inout float incoming) {
    float a=radiant_temperature(first),b=radiant_temperature(second),difference=radiant_fourth(a)-radiant_fourth(b);
    float coefficient=radiant_coefficient(first,second),dt=uintBitsToFloat(params.seconds);
    if(abs(difference)>0.&&dt>0.) coefficient=min(coefficient,abs(heat)/(dt*abs(difference)));
    outgoing+=coefficient;incoming+=coefficient*radiant_fourth(b);
}
void radiant_apply(uint index) {
    if(samples[index].y<=0.) return;
    float heat=-fluxes[index].x-fluxes[index].y,outgoing=0.,incoming=0.;
    for(uint axis=0u;axis<2u;axis++) {
        uint endpoint=floatBitsToUint(fluxes[index][axis+2u]);
        if(endpoint>0u) radiant_add_pair(index,endpoint-1u,fluxes[index][axis],outgoing,incoming);
        uint coordinate=axis==0u?index%params.width:index/params.width,stride=axis==0u?1u:params.width;
        for(uint distance=1u;distance<=min(coordinate,FR_THERMAL_RADIATION_RADIUS);distance++) {
            uint other=index-distance*stride;
            if(floatBitsToUint(fluxes[other][axis+2u])!=index+1u) continue;
            heat+=fluxes[other][axis];radiant_add_pair(index,other,fluxes[other][axis],outgoing,incoming);
        }
    }
    uint kind=floatBitsToUint(samples[index+params.width*params.height].x);
    if(kind==4u) fr_body_deposit_radiance(index,heat,outgoing,incoming);
    else if(kind==1u) {uvec2 h=handle_at(index);if(valid(h)) add_energy(Words(reactive_entry_address(uvec2(root[4],root[5]),h.x)),heat);}
    else if(kind==2u) {uint at=atlas_at(index);if(at!=0xffffffffu) water_energy[at]+=heat;}
}
