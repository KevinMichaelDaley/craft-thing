#ifndef DC_RUNTIME_BODY_STORAGE_GLSL
#define DC_RUNTIME_BODY_STORAGE_GLSL
#include "rigid_storage.glsl"
#ifndef DC_RUNTIME_BODY_BINDING
#define DC_RUNTIME_BODY_BINDING 4
#endif
/* Opt in per consumer: the Intel fracture compiler expands phase 4
   prohibitively with this declaration. Its root remains conservative. */
#ifdef DC_RUNTIME_ROOT_READONLY
layout(set=0,binding=DC_RUNTIME_BODY_BINDING,std430) readonly restrict buffer RuntimeBodyRoot {
#else
layout(set=0,binding=DC_RUNTIME_BODY_BINDING,std430) buffer RuntimeBodyRoot {
#endif
    BodyStorageRoot runtime_body_root;
};
layout(buffer_reference,std430,buffer_reference_align=4) buffer RuntimeBodyWord {uint value;};
#define BODY_CAPACITY (runtime_body_root.slab_count<<DC_BODY_SLAB_SHIFT)
#define DC_BODY(slot) DC_BODY_AT(runtime_body_root,slot)
#define DC_BODY_FREE(slot) (DC_BODY(slot).enabled==0u&&DC_BODY(slot).reserved0==0u&&DC_BODY_OWNER(runtime_body_root,slot).state!=DC_BODY_OWNER_RETIRED)
uvec2 runtime_body_word_address(uvec2 address,uint word) {
    uint carry;
    uint low=uaddCarry(address.x,word<<2u,carry);
    return uvec2(low,address.y+(word>>30u)+carry);
}
RuntimeBodyWord runtime_body_data_word(uint index) {
    if((index&0x80000000u)!=0u) {
        uint state=index&0x7fffffffu,slot=state/64u;
        return RuntimeBodyWord(runtime_body_word_address(DC_BODY_PAGE(runtime_body_root,slot).solver,
            (slot&(DC_BODY_SLAB_RECORDS-1u))*64u+state%64u));
    }
    return RuntimeBodyWord(runtime_body_word_address(runtime_body_root.data,index));
}
#define DC_DATA(index) BodyStorageWords(runtime_body_root.data).words[index]
#define DC_SOLVER_DATA(index) runtime_body_data_word(index).value
#define DC_BUFFER_DATA(index) DC_DATA(index)
#define DC_SUPPORT_DATA(index) DC_DATA(index)
#define DC_PIXEL_WORD(index) BodyStorageWords(runtime_body_root.geometry).words[index]
#define DC_RUNTIME_BODY_STATE(slot) (0x80000000u|((slot)*64u))
#define DC_BODY_CLIPPED(slot) DC_SOLVER_DATA(DC_RUNTIME_BODY_STATE(slot)+48u)
#define DC_BODY_DEPENDENCIES(slot) DC_SOLVER_DATA(DC_RUNTIME_BODY_STATE(slot)+49u)
#define DC_RUNTIME_WORK(index) BodyStorageWords(runtime_body_root.work).words[index]
bool runtime_component_anchors_enabled() {
    return DC_RUNTIME_WORK(DC_BODY_ANCHOR_MAGIC_WORD)==DC_BODY_ANCHOR_MAGIC;
}
#define DC_COMPONENT_ANCHOR_BITS(word) DC_RUNTIME_WORK(DC_RUNTIME_WORK(DC_BODY_ANCHOR_BITS_WORD)+(word))
bool runtime_component_anchored(uint root) {
    return runtime_component_anchors_enabled()&&
        (DC_COMPONENT_ANCHOR_BITS(root/32u)&(1u<<(root%32u)))!=0u;
}
#define DC_COMPONENT_CACHE(index) BodyStorageWords(uvec2(DC_RUNTIME_WORK(42u),DC_RUNTIME_WORK(43u))).words[index]
#define DC_CONTACT_WORK(index) BodyStorageWords(uvec2(DC_RUNTIME_WORK(45u),DC_RUNTIME_WORK(46u))).words[index]
#define DC_LAYOUT_PAIR_CAPACITY DC_RUNTIME_WORK(36u)
#define DC_LAYOUT_CONTACT_CAPACITY DC_RUNTIME_WORK(37u)
#define DC_RUNTIME_REQUEST(index) BodyStorageWords(runtime_body_root.requests).words[index]
RuntimeBodyWord runtime_hydro_word(uint index) {
    if((index&0x80000000u)!=0u) {
        uint value=index&0x7fffffffu,slot=value/128u;
        return RuntimeBodyWord(runtime_body_word_address(DC_BODY_PAGE(runtime_body_root,slot).auxiliary,
            (slot&63u)*128u+value%128u));
    }
    uint field=(index&0x40000000u)!=0u?3u:4u;
    uint relative=index&0x1fffffffu;
    return RuntimeBodyWord(runtime_body_word_address(runtime_body_root.work,DC_RUNTIME_WORK(field)+relative));
}
#define DC_HYDRO_WORD(index) runtime_hydro_word(index).value
layout(buffer_reference,std430,buffer_reference_align=8) buffer RuntimeBodyPoint {ivec2 value;};
#define DC_FRAGMENT_WORD(field,index) DC_RUNTIME_WORK(DC_RUNTIME_WORK(12u)+(field)*DC_RUNTIME_WORK(13u)+(index))
#define DC_FRAGMENT_POINT(field,index) RuntimeBodyPoint(runtime_body_word_address(runtime_body_root.work,DC_RUNTIME_WORK(12u)+(field)*DC_RUNTIME_WORK(13u)+2u*(index))).value
#define DC_FRAGMENT_LABEL(index) DC_FRAGMENT_WORD(10u,index)
#define DC_FRAGMENT_QUEUE(index) DC_FRAGMENT_WORD(11u,index)
#define DC_CARVE_ID(index) DC_RUNTIME_WORK(DC_RUNTIME_WORK(11u)+(index))
uint runtime_body_hash(uint id) {
    id^=id>>16u;id*=0x7feb352du;id^=id>>15u;id*=0x846ca68bu;return id^(id>>16u);
}
uint runtime_body_find(uint id) {
    if(id==0u) return BODY_CAPACITY;
    uint base=DC_RUNTIME_WORK(7u),count=DC_RUNTIME_WORK(8u),hash=runtime_body_hash(id)&(count-1u);
    for(uint probe=0u;probe<count;probe++) {
        uint at=base+2u*((hash+probe)&(count-1u)),key=DC_RUNTIME_WORK(at);
        if(key==0u) break;
        if(key==id) return DC_RUNTIME_WORK(at+1u);
    }
    return BODY_CAPACITY;
}
#endif
