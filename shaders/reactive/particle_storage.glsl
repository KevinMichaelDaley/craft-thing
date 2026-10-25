#ifndef FR_REACTIVE_PARTICLE_STORAGE_GLSL
#define FR_REACTIVE_PARTICLE_STORAGE_GLSL
#include "gpu_storage.glsl"
#ifdef DC_RUNTIME_BODY_STORAGE
bool reactive_particles_enabled() {
    return reactive_enabled()&&(FR_GPU_ROOT.words[14]|FR_GPU_ROOT.words[15])!=0u;
}
uvec2 reactive_particle_read(uint pool,uint index) {
    uint address=14u+pool*2u;
    return ReactiveHandles(uvec2(FR_GPU_ROOT.words[address],FR_GPU_ROOT.words[address+1u])).values[index];
}
void reactive_particle_write(uint pool,uint index,uvec2 handle) {
    uint address=14u+pool*2u;
    ReactiveHandles(uvec2(FR_GPU_ROOT.words[address],FR_GPU_ROOT.words[address+1u])).values[index]=handle;
}
void reactive_particle_gather(uint pool,uint source,uint target,bool occupied) {
    if(reactive_particles_enabled())
        reactive_particle_write(1u-pool,target,occupied?reactive_particle_read(pool,source):uvec2(0));
}
void reactive_particle_commit(uint pool,uint target) {
    if(reactive_particles_enabled()) reactive_particle_write(pool,target,reactive_particle_read(1u-pool,target));
}
bool reactive_leaf_reservation_ready(uint relative,uint nibble,uint capacity) {
    return !reactive_enabled()||!reactive_handle_valid(reactive_shape_read(relative,nibble,capacity))||reactive_particles_enabled();
}
void reactive_leaf_to_particle(uint relative,uint nibble,uint capacity,uint particle,uvec2 id) {
    if(!reactive_particles_enabled()) return;
    uvec2 handle=reactive_shape_read(relative,nibble,capacity);
    if(!reactive_handle_valid(handle)) return;
    reactive_entry_write(handle.x,FR_ENTRY_OWNER,3u);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+1u,id.x);
    reactive_entry_write(handle.x,FR_ENTRY_OWNER+2u,id.y);
    for(uint field=3u;field<=5u;field++) reactive_entry_write(handle.x,FR_ENTRY_OWNER+field,0u);
    reactive_entry_write(handle.x,FR_ENTRY_STATE+FR_STATE_MATERIAL,10u);
    reactive_particle_write(0u,particle,handle);reactive_particle_write(1u,particle,uvec2(0));
    reactive_shape_write(relative,nibble,capacity,uvec2(0));
}
#else
void reactive_particle_gather(uint pool,uint source,uint target,bool occupied) {}
void reactive_particle_commit(uint pool,uint target) {}
bool reactive_leaf_reservation_ready(uint relative,uint nibble,uint capacity) {return true;}
void reactive_leaf_to_particle(uint relative,uint nibble,uint capacity,uint particle,uvec2 id) {}
#endif
#endif
