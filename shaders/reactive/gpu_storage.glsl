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
#else
void reactive_forget_cell(uint cell) {}
#endif
#endif
