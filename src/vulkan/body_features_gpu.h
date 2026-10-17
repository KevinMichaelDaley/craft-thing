#ifndef DC_BODY_FEATURES_GPU_H
#define DC_BODY_FEATURES_GPU_H
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
typedef struct {
    VkPhysicalDeviceSynchronization2Features synchronization;
    VkPhysicalDeviceBufferDeviceAddressFeatures addresses;
} dc_gpu_body_features_t;
/** Query required body-storage features, preserving output if unsupported.
 * The successful chain belongs to out and may be passed to VkDeviceCreateInfo. */
static inline bool dc_gpu_body_features_query(VkPhysicalDevice physical,
    dc_gpu_body_features_t *out,char *err,uint32_t cap) {
    (void)physical;(void)out;(void)err;(void)cap;return false;
}
#endif
