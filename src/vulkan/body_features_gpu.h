#ifndef DC_BODY_FEATURES_GPU_H
#define DC_BODY_FEATURES_GPU_H
#include <stdbool.h>
#include <stdio.h>
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
    if(!physical||!out) {
        if(err&&cap) snprintf(err,cap,"Invalid body feature query destination");
        return false;
    }
    dc_gpu_body_features_t available={
        .synchronization={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES},
        .addresses={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES}};
    available.synchronization.pNext=&available.addresses;
    VkPhysicalDeviceFeatures2 features={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext=&available.synchronization};
    vkGetPhysicalDeviceFeatures2(physical,&features);
    const char *missing=!available.synchronization.synchronization2?"Device lacks synchronization2":
        !available.addresses.bufferDeviceAddress?"Device lacks bufferDeviceAddress":NULL;
    if(missing) {if(err&&cap) snprintf(err,cap,"%s",missing);return false;}
    *out=(dc_gpu_body_features_t){
        .synchronization={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,.synchronization2=VK_TRUE},
        .addresses={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,.bufferDeviceAddress=VK_TRUE}};
    out->synchronization.pNext=&out->addresses;
    return true;
}
#endif
