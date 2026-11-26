#include <stdio.h>

#include "dungeoncraft/gpu.h"

static dc_gpu_t *live_context;

bool __real_dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                         const char *shader, char *err, uint32_t cap);
void __real_dc_gpu_destroy(dc_gpu_t *gpu);

/** Enforce a one-context budget for the fixture's independent comparison legs. */
bool __wrap_dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                         const char *shader, char *err, uint32_t cap) {
    if (live_context) {
        if (out) *out = NULL;
        if (err && cap) snprintf(err, cap, "Halo fixture already owns a Vulkan context");
        return false;
    }
    if (!__real_dc_gpu_create(out, width, height, shader, err, cap)) return false;
    live_context = *out;
    return true;
}

/** Track only completed allocations returned to a fixture. */
void __wrap_dc_gpu_destroy(dc_gpu_t *gpu) {
    if (gpu == live_context) live_context = NULL;
    __real_dc_gpu_destroy(gpu);
}
