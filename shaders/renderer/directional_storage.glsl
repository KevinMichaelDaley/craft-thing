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
struct Field {vec4 l0;vec4 lx;vec4 ly;vec4 lz;};
struct PackedField {uint r,g,b,dr,dg,db;};
Field renderer_unpack_field(PackedField packed) {
    vec3 energy=uintBitsToFloat(uvec3(packed.r,packed.g,packed.b));
    vec3 r=renderer_unpack_direction(packed.dr)*energy.r;
    vec3 g=renderer_unpack_direction(packed.dg)*energy.g;
    vec3 b=renderer_unpack_direction(packed.db)*energy.b;
    return Field(vec4(energy,0),vec4(r.x,g.x,b.x,0),
                 vec4(r.y,g.y,b.y,0),vec4(r.z,g.z,b.z,0));
}
PackedField renderer_pack_field(Field field) {
    vec3 denominator=max(field.l0.rgb,vec3(1e-30));
    uvec3 energy=floatBitsToUint(field.l0.rgb);
    return PackedField(energy.r,energy.g,energy.b,
        renderer_pack_direction(vec3(field.lx.r,field.ly.r,field.lz.r)/denominator.r),
        renderer_pack_direction(vec3(field.lx.g,field.ly.g,field.lz.g)/denominator.g),
        renderer_pack_direction(vec3(field.lx.b,field.ly.b,field.lz.b)/denominator.b));
}
#endif
