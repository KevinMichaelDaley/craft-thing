#ifndef DC_SUBGROUP_GPU_H
#define DC_SUBGROUP_GPU_H
#include <vulkan/vulkan.h>

/** Return a supported sixteen-lane compute subgroup, or zero for driver choice.
 * Device creation must enable subgroupSizeControl when available. */
static inline uint32_t dc_gpu_transport_subgroup_size(VkPhysicalDevice physical) {
    VkPhysicalDeviceSubgroupSizeControlFeatures control = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &control};
    vkGetPhysicalDeviceFeatures2(physical, &features);
    VkPhysicalDeviceSubgroupSizeControlProperties limits = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &limits};
    vkGetPhysicalDeviceProperties2(physical, &properties);
    return control.subgroupSizeControl && limits.minSubgroupSize <= 16u &&
           limits.maxSubgroupSize >= 16u &&
           (limits.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT)
               ? 16u : 0u;
}
#endif
