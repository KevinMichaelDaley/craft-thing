#define FR_GAS_COMPACT_WORK 1
#define FR_GAS_FORCE_PRACTICAL 1
#define FR_GAS_FUSED_AXIS 1
#define FR_GAS_FACTOR_ENTRY gas_factor_run
#define FR_GAS_PREFIX_ENTRY gas_prefix_run
#include "transport_factor_body.glsl"
#include "transport_prefix_body.glsl"
void main() {
    transport_pass=0u;gas_factor_run();memoryBarrierBuffer();barrier();
    gas_prefix_run();memoryBarrierBuffer();barrier();
    transport_pass=1u;gas_prefix_run();
#ifndef FR_GAS_COMPACT_IMPLICIT_LINES
    if(transport_direct_first_axis()&&transport_work[gl_WorkGroupID.x*params.width].mass.w==-1.) {
        for(uint cell=gl_LocalInvocationIndex;cell<params.width;cell+=FR_GAS_LOCAL_SIZE) {
            uint atlas=gas_atlas(ivec2(int(cell),int(gl_WorkGroupID.x)));
            if(atlas!=0xffffffffu) gas_write_provisional(atlas,transport_axis_input(atlas));
        }
    }
#endif
}
