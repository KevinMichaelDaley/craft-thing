#ifndef DC_BODY_ADDRESS_GPU_H
#define DC_BODY_ADDRESS_GPU_H
#include "gpu_internal.h"
typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    void *mapped;
    VkDeviceSize bytes,allocation_bytes;
    bool host_visible,device_local;
} dc_gpu_address_buffer_t;
/** Allocate an addressable resource after bufferDeviceAddress is enabled.
 * Failure preserves the destination and releases every partial allocation. */
bool dc_gpu_address_buffer_create(dc_gpu_t *gpu,VkDeviceSize bytes,bool host_visible,
    dc_gpu_address_buffer_t *out,char *err,uint32_t cap);
/** Release an address resource only after its final GPU use has completed. */
void dc_gpu_address_buffer_destroy(dc_gpu_t *gpu,dc_gpu_address_buffer_t *buffer);
#endif
