#ifndef DC_VULKAN_FLUID_TRANSPORT_LIMITS_H
#define DC_VULKAN_FLUID_TRANSPORT_LIMITS_H

#include <stdint.h>

static inline uint32_t dc_gpu_transport_color_cache_cells(uint32_t length,
                                                         uint32_t shared_bytes) {
    /* State/request words plus two rounded bitsets; up to 1024 cells also
       cache their complete, unquantized pigment record. */
    uint32_t limit=shared_bytes<37u*1024u?shared_bytes/37u:
        (shared_bytes-16u*1024u)/21u;
    if(limit>2048u) limit=2048u;
    return length<limit?length:limit;
}

static inline uint32_t dc_gpu_transport_cache_cells(uint32_t length,
                                                   uint32_t shared_bytes) {
    uint32_t limit=shared_bytes/21u;
    if(limit>2048u) limit=2048u;
    return length<limit?length:limit;
}

#endif
