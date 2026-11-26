#define FR_GAS_COMPACT_WORK 1
#define FR_GAS_FORCE_PRACTICAL 1
#define FR_GAS_FUSED_AXIS 1
#define FR_GAS_FACTOR_ENTRY gas_factor_run
#define FR_GAS_PREFIX_ENTRY gas_prefix_run
#define FR_GAS_EXPLICIT_ENTRY gas_explicit_run
#define FR_GAS_REUSE_LINE_SHARED 1
#include "transport_implicit.glsl"
shared vec4 line_species[FR_GAS_EXPLICIT_LINE_CAPACITY];
#include "transport_factor_body.glsl"
#include "transport_prefix_body.glsl"
#include "transport_explicit_body.glsl"
void main() {
    gas_explicit_run();memoryBarrierBuffer();barrier();
    transport_pass=0u;gas_factor_run();memoryBarrierBuffer();barrier();
    gas_prefix_run();memoryBarrierBuffer();barrier();
    transport_pass=1u;gas_prefix_run();
    transport_preserve_skipped_first_axis();
}
