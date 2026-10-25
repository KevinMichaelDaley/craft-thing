#ifndef FR_REACTIVE_GPU_STORAGE_GLSL
#define FR_REACTIVE_GPU_STORAGE_GLSL
#include "../../include/dungeoncraft/reactive_state_layout.h"
#ifdef DC_RUNTIME_BODY_STORAGE
bool reactive_enabled() {return DC_RUNTIME_WORK(64u)==0x46525331u;}
layout(buffer_reference,std430,buffer_reference_align=4) coherent buffer ReactiveWords {uint words[];};
layout(buffer_reference,std430,buffer_reference_align=8) coherent buffer ReactiveHandles {uvec2 values[];};
#define FR_GPU_ROOT ReactiveWords(uvec2(DC_RUNTIME_WORK(65u),DC_RUNTIME_WORK(66u)))
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
    for(uint nibble=0u;nibble<8u;nibble++) reactive_shape_write(target,nibble,capacity,reactive_shape_read(source,nibble,capacity));
}
void reactive_shape_clear(uint relative,uint nibble,uint capacity) {
    if(reactive_enabled()) reactive_shape_write(relative,nibble,capacity,uvec2(0));
}
void reactive_extract_cell(uint cell,uint relative,uint nibble,uint capacity,uint id,uint generation,uvec2 rest) {
    if(!reactive_enabled()) return;
    uvec2 handle=reactive_handle_read(cell);
    if(!reactive_handle_valid(handle)) return;
    reactive_entry_write(handle.x,FR_ENTRY_OWNER,2u);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+1u,id);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+2u,generation);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+3u,rest.x);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+4u,rest.y);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+5u,0u);
    reactive_shape_write(relative,nibble,capacity,handle);
    reactive_handle_write(cell,uvec2(0));
}
#else
void reactive_forget_cell(uint cell) {}
void reactive_shape_copy(uint target,uint source,uint capacity) {}
void reactive_shape_clear(uint relative,uint nibble,uint capacity) {}
void reactive_extract_cell(uint cell,uint relative,uint nibble,uint capacity,uint id,uint generation,uvec2 rest) {}
#endif
#endif
