#ifndef DC_SHAPE_COLORS_GLSL
#define DC_SHAPE_COLORS_GLSL
#include "../reactive/gpu_storage.glsl"
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
#ifdef DC_SHAPE_COLOR_PROFILE
    atomicAdd(color_profile[1],1u);
#endif
#ifdef DC_RUNTIME_BODY_STORAGE
    return DC_RUNTIME_WORK(29u);
#else
    return 2u*((max(params.width*params.height,color_slots()*4096u)+7u)/8u)+64u*8u;
#endif
}
uint color_pool_word() {
#ifdef DC_SHAPE_COLOR_PROFILE
    atomicAdd(color_profile[0],1u);
#endif
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
    reactive_shape_copy(target-color_pool_word(),source-color_pool_word(),color_capacity());
    for(uint nibble=0u;nibble<8u;nibble++) shape_color_set(target,nibble,shape_color_get(source,nibble));
}
void shape_color_stage(uint target,uint source) {
    vec4 marker=vec4(0);marker.w=uintBitsToFloat(0x80000000u|(source==0u?0u:source-color_pool_word()+1u));
    shape_color_set(target,0u,marker);
}
void shape_color_resolve(uint word) {
    uint pool=color_pool_word(),capacity=color_capacity();
    uint relative=word-pool,target=(relative%capacity)*8u;
#ifdef DC_RUNTIME_BODY_STORAGE
    uvec2 target_address=relative<capacity?uvec2(DC_RUNTIME_WORK(32u),DC_RUNTIME_WORK(33u)):
        uvec2(DC_RUNTIME_WORK(34u),DC_RUNTIME_WORK(35u));
    RuntimeShapeColors destination=RuntimeShapeColors(target_address);
    uint marker=floatBitsToUint(destination.colors[target].w);
#else
    uint marker=floatBitsToUint(relative<capacity?shape_colors_a[target].w:shape_colors_b[target].w);
#endif
    if((marker&0x80000000u)==0u) return;
    uint source=marker&0x7fffffffu;
    uint source_relative=source==0u?0u:source-1u,source_index=(source_relative%capacity)*8u;
#ifdef DC_RUNTIME_BODY_STORAGE
    uvec2 source_address=source_relative<capacity?uvec2(DC_RUNTIME_WORK(32u),DC_RUNTIME_WORK(33u)):
        uvec2(DC_RUNTIME_WORK(34u),DC_RUNTIME_WORK(35u));
    RuntimeShapeColors origin=RuntimeShapeColors(source_address);
    for(uint nibble=0u;nibble<8u;nibble++)
        destination.colors[target+nibble]=source==0u?vec4(0):origin.colors[source_index+nibble];
#else
    for(uint nibble=0u;nibble<8u;nibble++) {
        vec4 color=source==0u?vec4(0):source_relative<capacity?
            shape_colors_a[source_index+nibble]:shape_colors_b[source_index+nibble];
        if(relative<capacity) shape_colors_a[target+nibble]=color;
        else shape_colors_b[target+nibble]=color;
    }
#endif
}
#ifdef DC_COOPERATIVE_SHAPE_COLORS
/* Each eight-lane team owns one packed word. All lanes must enter, including
 * padding teams, so marker reads complete before alpha is overwritten. */
void shape_color_resolve_cooperative(uint word,bool enabled) {
    if(gl_SubgroupSize<8u) {
        if(enabled&&gl_LocalInvocationIndex%8u==0u) shape_color_resolve(word);
        return;
    }
    uint nibble=gl_LocalInvocationIndex%8u;
    uint relative=0u,capacity=1u,marker=0u;
    if(enabled&&nibble==0u) {
        relative=word-color_pool_word();capacity=color_capacity();
        uint target=(relative%capacity)*8u;
        marker=floatBitsToUint(relative<capacity?shape_colors_a[target].w:shape_colors_b[target].w);
    }
    uint leader=gl_SubgroupInvocationID&~7u;
    relative=subgroupShuffle(relative,leader);
    capacity=subgroupShuffle(capacity,leader);
    marker=subgroupShuffle(marker,leader);
    subgroupMemoryBarrierBuffer();subgroupBarrier();
    if(enabled&&(marker&0x80000000u)!=0u) {
        uint source=marker&0x7fffffffu,source_relative=source==0u?0u:source-1u;
        uint target=(relative%capacity)*8u+nibble,origin=(source_relative%capacity)*8u+nibble;
        vec4 color=source==0u?vec4(0):source_relative<capacity?shape_colors_a[origin]:shape_colors_b[origin];
        if(relative<capacity) shape_colors_a[target]=color;else shape_colors_b[target]=color;
#ifdef DC_SHAPE_COLOR_PROFILE
        atomicAdd(color_profile[2],1u);
#endif
    }
}
void shape_color_resolve_range(uint first,uint count) {
    uint teams=gl_WorkGroupSize.x/8u;
    for(uint base=0u;base<count;base+=teams) {
        uint word=base+gl_LocalInvocationIndex/8u;
        shape_color_resolve_cooperative(first+word,word<count);
    }
}
#endif
#endif
