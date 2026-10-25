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
#else
void reactive_particle_gather(uint pool,uint source,uint target,bool occupied) {}
void reactive_particle_commit(uint pool,uint target) {}
#endif
#endif
