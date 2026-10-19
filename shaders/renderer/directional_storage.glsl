#ifndef FR_RENDERER_DIRECTIONAL_STORAGE_GLSL
#define FR_RENDERER_DIRECTIONAL_STORAGE_GLSL
uint renderer_pack_direction(vec3 direction) {
    uvec3 value=uvec3(ivec3(round(clamp(direction,-1.,1.)*511.)))&1023u;
    return value.x|(value.y<<10u)|(value.z<<20u);
}
vec3 renderer_unpack_direction(uint packed) {
    return vec3(bitfieldExtract(int(packed),0,10),
                bitfieldExtract(int(packed),10,10),
                bitfieldExtract(int(packed),20,10))/511.;
}
#endif
