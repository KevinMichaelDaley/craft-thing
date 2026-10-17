/* Explicit experiment binary only; ordinary runtime pipelines stay unchanged. */
#include <stdint.h>
#include "../../src/vulkan/gpu_internal.h"

static uint32_t selected_modes;

void __real_dc_gpu_record_mpm(dc_gpu_t *gpu);
void __wrap_dc_gpu_record_mpm(dc_gpu_t *gpu) {
    __real_dc_gpu_record_mpm(gpu);
}

uint32_t diagnostic_mpm_selected_modes(void) {
    return selected_modes;
}
