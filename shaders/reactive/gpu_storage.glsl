#ifndef FR_REACTIVE_GPU_STORAGE_GLSL
#define FR_REACTIVE_GPU_STORAGE_GLSL
#include "../../include/dungeoncraft/reactive_state_layout.h"
#ifdef DC_RUNTIME_BODY_STORAGE
bool reactive_enabled() {return DC_RUNTIME_WORK(64u)==0x46525331u;}
/* Match the index pass that follows publication, including reused hot slots.
 * The structural fracture lineage generation is a different identity. */
uint reactive_body_generation(uint slot,uint id) {
    BodyStorageOwner owner=DC_BODY_OWNER(runtime_body_root,slot);
    if(owner.runtime_id!=id&&owner.state!=DC_BODY_OWNER_RESERVED&&owner.generation!=0xffffffffu)
        return owner.generation+1u;
    return owner.generation;
}
layout(buffer_reference,std430,buffer_reference_align=4) coherent buffer ReactiveWords {uint words[];};
layout(buffer_reference,std430,buffer_reference_align=4) readonly restrict buffer ReactiveRootWords {uint words[];};
layout(buffer_reference,std430,buffer_reference_align=8) coherent buffer ReactiveHandles {uvec2 values[];};
#define FR_GPU_ROOT ReactiveRootWords(uvec2(DC_RUNTIME_WORK(65u),DC_RUNTIME_WORK(66u)))
ReactiveWords reactive_entries() {
    ReactiveRootWords root=FR_GPU_ROOT;
    return ReactiveWords(uvec2(root.words[4],root.words[5]));
}
ReactiveHandles reactive_shape_buffer(uint relative,uint capacity) {
    ReactiveRootWords root=FR_GPU_ROOT;uint address=relative<capacity?8u:10u;
    return ReactiveHandles(uvec2(root.words[address],root.words[address+1u]));
}
void reactive_owner_write(uint entry,uint kind,uvec4 key,uint last) {
    ReactiveWords entries=reactive_entries();uint at=entry*FR_ENTRY_WORDS+FR_ENTRY_OWNER;
    entries.words[at]=kind;entries.words[at+1u]=key.x;entries.words[at+2u]=key.y;
    entries.words[at+3u]=key.z;entries.words[at+4u]=key.w;entries.words[at+5u]=last;
}
uint reactive_entry_capacity() {return FR_GPU_ROOT.words[2];}
uint reactive_entry_read(uint entry,uint field) {
    return ReactiveWords(uvec2(FR_GPU_ROOT.words[4],FR_GPU_ROOT.words[5])).words[entry*FR_ENTRY_WORDS+field];
}
void reactive_entry_write(uint entry,uint field,uint value) {
    ReactiveWords(uvec2(FR_GPU_ROOT.words[4],FR_GPU_ROOT.words[5])).words[entry*FR_ENTRY_WORDS+field]=value;
}
uvec2 reactive_handle_read(uint cell) {
    return ReactiveHandles(uvec2(FR_GPU_ROOT.words[6],FR_GPU_ROOT.words[7])).values[cell];
}
void reactive_handle_write(uint cell,uvec2 handle) {
    ReactiveHandles(uvec2(FR_GPU_ROOT.words[6],FR_GPU_ROOT.words[7])).values[cell]=handle;
}
#include "state.glsl"
void reactive_forget_cell(uint cell) {if(reactive_enabled()) reactive_erase(cell);}
uvec2 reactive_shape_read(uint relative,uint nibble,uint capacity) {
    uint address=relative<capacity?8u:10u;
    return ReactiveHandles(uvec2(FR_GPU_ROOT.words[address],FR_GPU_ROOT.words[address+1u])).values[(relative%capacity)*8u+nibble];
}
void reactive_shape_write(uint relative,uint nibble,uint capacity,uvec2 handle) {
    uint address=relative<capacity?8u:10u;
    ReactiveHandles(uvec2(FR_GPU_ROOT.words[address],FR_GPU_ROOT.words[address+1u])).values[(relative%capacity)*8u+nibble]=handle;
}
void reactive_shape_copy(uint target,uint source,uint capacity) {
    if(!reactive_enabled()) return;
    ReactiveHandles destination=reactive_shape_buffer(target,capacity),origin=reactive_shape_buffer(source,capacity);
    uint to=(target%capacity)*8u,from=(source%capacity)*8u;
    for(uint nibble=0u;nibble<8u;nibble++) destination.values[to+nibble]=origin.values[from+nibble];
}
void reactive_shape_clear(uint relative,uint nibble,uint capacity) {
    if(reactive_enabled()) reactive_shape_write(relative,nibble,capacity,uvec2(0));
}
void reactive_shape_stage(uint target,uint source,uint capacity) {
    if(reactive_enabled()) reactive_shape_write(target,0u,capacity,uvec2(source==0xffffffffu?source:source+1u,0u));
}
void reactive_shape_resolve(uint target,uint capacity,uint packed) {
    if(!reactive_enabled()) return;
    ReactiveHandles destination=reactive_shape_buffer(target,capacity);uint to=(target%capacity)*8u;
    uvec2 marker=destination.values[to];
    if(marker.y!=0u||marker.x==0u) return;
    uint source=marker.x==0xffffffffu?0u:marker.x-1u,from=(source%capacity)*8u;
    ReactiveHandles origin=reactive_shape_buffer(source,capacity);
    for(uint nibble=0u;nibble<8u;nibble++) {
        uvec2 handle=marker.x==0xffffffffu||((packed>>(nibble*4u))&15u)==0u?
            uvec2(0):origin.values[from+nibble];
        destination.values[to+nibble]=handle;
    }
}
void reactive_shape_reown(uint relative,uint nibble,uint capacity,uint id,uint generation,uvec2 rest) {
    uvec2 handle=reactive_shape_read(relative,nibble,capacity);
    if(!reactive_handle_valid(handle)) return;
    reactive_owner_write(handle.x,2u,uvec4(id,generation,rest),0u);
}
void reactive_shape_erase(uint relative,uint nibble,uint capacity) {
    uvec2 handle=reactive_shape_read(relative,nibble,capacity);
    if(!reactive_handle_valid(handle)) return;
    ReactiveWords entries=reactive_entries();uint at=handle.x*FR_ENTRY_WORDS;
    entries.words[at+FR_ENTRY_OWNER]=0u;entries.words[at+FR_ENTRY_GENERATION]=min(handle.y,0xfffffffeu)+1u;
    reactive_shape_write(relative,nibble,capacity,uvec2(0));
}
void reactive_extract_cell(uint cell,uint relative,uint nibble,uint capacity,uint id,uint generation,uvec2 rest) {
    if(!reactive_enabled()) return;
    uvec2 handle=reactive_handle_read(cell);
    if(!reactive_handle_valid(handle)) return;
    reactive_owner_write(handle.x,2u,uvec4(id,generation,rest),0u);
    reactive_shape_write(relative,nibble,capacity,handle);
    reactive_handle_write(cell,uvec2(0));
}
#else
bool reactive_enabled() {return false;}
uint reactive_body_generation(uint slot,uint id) {return 1u;}
void reactive_forget_cell(uint cell) {}
void reactive_shape_copy(uint target,uint source,uint capacity) {}
void reactive_shape_clear(uint relative,uint nibble,uint capacity) {}
void reactive_extract_cell(uint cell,uint relative,uint nibble,uint capacity,uint id,uint generation,uvec2 rest) {}
void reactive_shape_stage(uint target,uint source,uint capacity) {}
void reactive_shape_resolve(uint target,uint capacity,uint packed) {}
#endif
#endif
