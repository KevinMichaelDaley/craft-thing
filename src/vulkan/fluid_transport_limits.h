#ifndef DC_VULKAN_FLUID_TRANSPORT_LIMITS_H
#define DC_VULKAN_FLUID_TRANSPORT_LIMITS_H

#include <stdint.h>

static inline uint32_t dc_gpu_transport_cache_cells(uint32_t length,
                                                   uint32_t shared_bytes) {
    /* Four state words, one request word and a rounded capacity bitset.
       Twenty-one bytes per cell conservatively covers the bitset rounding. */
    uint32_t limit=shared_bytes/21u;
    if(limit>2048u) limit=2048u;
    return length<limit?length:limit;
}

#endif
