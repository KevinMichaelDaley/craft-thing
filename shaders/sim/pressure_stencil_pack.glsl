#ifndef DC_PRESSURE_STENCIL_PACK_GLSL
#define DC_PRESSURE_STENCIL_PACK_GLSL
/* Apertures are integer multiples of 2^-16 in [0,1]. Their low eight float
 * bits are zero, and every nonzero exponent has bits 28 and 29 set. */
uvec4 pack_pressure_stencil(uvec4 weights,uint divergence,bool cached) {
    uvec4 packed;
    for(int axis=0;axis<4;axis++)
        packed[axis]=(weights[axis]&0xffffff00u)|((divergence>>uint(axis*8))&255u);
    packed.x=(packed.x&~0x20000000u)|(cached?0x20000000u:0u);
    return packed;
}
uint unpack_pressure_weight(uvec4 packed,int axis) {
    uint word=packed[axis],value=word&0x1fffff00u;
    if((value&0x10000000u)!=0u) value|=0x20000000u;
    return (word&0xc0000000u)|value;
}
bool pressure_divergence_cached(uvec4 packed) {return (packed.x&0x20000000u)!=0u;}
uint unpack_pressure_divergence(uvec4 packed) {
    return (packed.x&255u)|((packed.y&255u)<<8u)|((packed.z&255u)<<16u)|((packed.w&255u)<<24u);
}
#endif
