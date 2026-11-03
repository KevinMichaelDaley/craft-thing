#include <stdio.h>
#include <stdlib.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_pick_device(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap) {
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(gpu->instance, &count, NULL) != VK_SUCCESS || !count)
        return error(err, cap, "No Vulkan device found");
    VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
    if (!devices) return error(err, cap, "Out of memory listing Vulkan devices");
    VkResult result = vkEnumeratePhysicalDevices(gpu->instance, &count, devices);
    int best_score = -1;
    for (uint32_t d = 0; d < count && result == VK_SUCCESS; ++d) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[d], &props);
        if (props.apiVersion < VK_API_VERSION_1_3 || props.limits.maxStorageBufferRange < bytes) continue;
        uint32_t n = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &n, NULL);
        VkQueueFamilyProperties *families = calloc(n, sizeof(*families));
        if (!families) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &n, families);
        for (uint32_t i = 0; i < n; ++i) {
            VkBool32 can_present = VK_FALSE;
            if (gpu->surface) vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], i, gpu->surface, &can_present);
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                (!gpu->surface || can_present)) {
                int score = dc_gpu_device_score(&props, props.limits.maxStorageBufferRange,
                                                bytes, true, true,
                                                gpu->surface ? can_present==VK_TRUE : true);
                if (score > best_score) {
                    best_score = score;
                    gpu->physical = devices[d];
                    gpu->family = i;
                    gpu->device_type = props.deviceType;
                    gpu->vendor_id = props.vendorID;
                    snprintf(gpu->device_name, sizeof(gpu->device_name), "%s", props.deviceName);
                }
                break;
            }
        }
        free(families);
    }
    free(devices);
    if (!gpu->physical) return error(err, cap, "No Vulkan 1.3 compute device supports this grid");
    return true;
}
int dc_gpu_device_score(const VkPhysicalDeviceProperties *properties,
                        VkDeviceSize max_storage, VkDeviceSize required,
                        bool compute, bool transfer, bool present) {
    if (!properties || properties->apiVersion < VK_API_VERSION_1_3 ||
        max_storage < required || !compute || !transfer || !present ||
        properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        return -1;
    switch (properties->deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
    default: return 1;
    }
}
