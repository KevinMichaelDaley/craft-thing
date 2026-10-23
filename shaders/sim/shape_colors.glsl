#ifndef DC_SHAPE_COLORS_GLSL
#define DC_SHAPE_COLORS_GLSL
layout(set=0,binding=33,std430) buffer TerrainColors {vec4 terrain_colors[];};
#ifdef DC_RUNTIME_BODY_STORAGE
layout(buffer_reference,std430,buffer_reference_align=16) coherent buffer RuntimeShapeColors {vec4 colors[];};
#define shape_colors_a RuntimeShapeColors(uvec2(DC_RUNTIME_WORK(32u),DC_RUNTIME_WORK(33u))).colors
#define shape_colors_b RuntimeShapeColors(uvec2(DC_RUNTIME_WORK(34u),DC_RUNTIME_WORK(35u))).colors
#else
layout(set=0,binding=34,std430) coherent buffer ShapeColorsA {vec4 shape_colors_a[];};
layout(set=0,binding=35,std430) coherent buffer ShapeColorsB {vec4 shape_colors_b[];};
#endif
uint color_slots() {
    uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
    uint base=8u+2u*w*h+DC_LAYOUT_PAIR_CAPACITY*8u+12u+DC_LAYOUT_CONTACT_CAPACITY*32u+16u+64u*32u+DC_LAYOUT_CONTACT_CAPACITY*16u;
    return pixel_word_at(BODY_STORAGE_WORDS+base+1u);
}
uint color_capacity() {
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_WORK(29u);
#else
    return 2u*((max(params.width*params.height,color_slots()*4096u)+7u)/8u)+64u*8u;
#endif
}
uint color_pool_word() {
    uint w=(params.width+63u)/64u+2u,h=(params.height+63u)/64u+2u;
    return BODY_STORAGE_WORDS+8u+2u*w*h+DC_LAYOUT_PAIR_CAPACITY*8u+12u+DC_LAYOUT_CONTACT_CAPACITY*32u+16u+64u*32u+DC_LAYOUT_CONTACT_CAPACITY*16u+
        32u+color_slots()*8u+40000u+16u;
}
vec4 shape_color_get(uint word,uint nibble) {
    uint relative=word-color_pool_word(),capacity=color_capacity(),index=(relative%capacity)*8u+nibble;
    return relative<capacity?shape_colors_a[index]:shape_colors_b[index];
}
void shape_color_set(uint word,uint nibble,vec4 color) {
    uint relative=word-color_pool_word(),capacity=color_capacity(),index=(relative%capacity)*8u+nibble;
    if(relative<capacity) shape_colors_a[index]=color;else shape_colors_b[index]=color;
}
void shape_color_copy_word(uint target,uint source) {
    for(uint nibble=0u;nibble<8u;nibble++) shape_color_set(target,nibble,shape_color_get(source,nibble));
}
void shape_color_stage(uint target,uint source) {
    vec4 marker=vec4(0);marker.w=uintBitsToFloat(0x80000000u|(source==0u?0u:source-color_pool_word()+1u));
    shape_color_set(target,0u,marker);
}
void shape_color_resolve(uint word) {
    uint marker=floatBitsToUint(shape_color_get(word,0u).w);
    if((marker&0x80000000u)==0u) return;
    uint source=marker&0x7fffffffu;
    for(uint nibble=0u;nibble<8u;nibble++)
        shape_color_set(word,nibble,source==0u?vec4(0):shape_color_get(color_pool_word()+source-1u,nibble));
}
#endif
