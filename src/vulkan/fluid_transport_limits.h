#ifndef DC_VULKAN_FLUID_TRANSPORT_LIMITS_H
#define DC_VULKAN_FLUID_TRANSPORT_LIMITS_H

#include <stdint.h>

static inline uint32_t dc_gpu_transport_cache_cells(uint32_t length,
                                                   uint32_t shared_bytes) {
    (void)shared_bytes;
    return length < 2048u ? length : 2048u;
}

#endif
