#define FR_GAS_COMPACT_WORK 1
#define FR_GAS_FUSED_AXIS 1
#define FR_GAS_FACTOR_ENTRY gas_factor_run
#define FR_GAS_PREFIX_ENTRY gas_prefix_run
#include "transport_factor_body.glsl"
#include "transport_prefix_body.glsl"
void main() {
    transport_pass=0u;gas_factor_run();memoryBarrierBuffer();barrier();
    gas_prefix_run();memoryBarrierBuffer();barrier();
    transport_pass=1u;gas_prefix_run();
}
