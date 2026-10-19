#ifndef DC_LEAF_COLOR_GLSL
#define DC_LEAF_COLOR_GLSL
#include "../../include/dungeoncraft/leaf_material.h"

uint leaf_color_pack(vec3 linear) {
    linear=clamp(linear,vec3(0),vec3(1));
    vec3 srgb=mix(1.055*pow(linear,vec3(1./2.4))-.055,12.92*linear,
        lessThanEqual(linear,vec3(.0031308)));
    uvec3 rgb=uvec3(round(srgb*255.));
    return DC_LEAF_COLOR_FLAG|rgb.x|(rgb.y<<8u)|(rgb.z<<16u);
}
vec3 leaf_color_unpack(uint flags) {
    vec3 srgb=vec3(flags&255u,(flags>>8u)&255u,(flags>>16u)&255u)/255.;
    return mix(pow((srgb+.055)/1.055,vec3(2.4)),srgb/12.92,
        lessThanEqual(srgb,vec3(.04045)));
}
#endif
