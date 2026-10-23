#include <stdbool.h>
#include <stddef.h>

#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

static int g_pass,g_fail;
static bool supported,requested;
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
static bool address_supported=true,address_requested,int64_requested;
#endif
static unsigned device_calls;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *i,const VkAllocationCallbacks *a,VkInstance *out) {
    (void)i;(void)a;*out=(VkInstance)(uintptr_t)1;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance i,uint32_t *count,VkPhysicalDevice *out) {
    (void)i;*count=1;if(out)out[0]=(VkPhysicalDevice)(uintptr_t)2;return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice d,VkPhysicalDeviceProperties *p) {
    (void)d;memset(p,0,sizeof(*p));p->vendorID=0x8086;p->deviceType=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    p->apiVersion=VK_API_VERSION_1_3;snprintf(p->deviceName,sizeof(p->deviceName),"CPU-only mock");
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice d,uint32_t *count,VkQueueFamilyProperties *p) {
    (void)d;*count=1;if(p)*p=(VkQueueFamilyProperties){.queueFlags=VK_QUEUE_COMPUTE_BIT,.queueCount=1};
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(VkPhysicalDevice d,VkPhysicalDeviceFeatures2 *p) {
    (void)d;
    for(void *node=p->pNext;node;) {
        VkBaseOutStructure header;memcpy(&header,node,sizeof(header));
        if(header.sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
            VkBool32 value=supported;
            memcpy((char *)node+offsetof(VkPhysicalDeviceVulkan13Features,synchronization2),&value,sizeof(value));
        }
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
        if(header.sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            VkBool32 value=address_supported;
            memcpy((char *)node+offsetof(VkPhysicalDeviceVulkan12Features,bufferDeviceAddress),&value,sizeof(value));
        }
#endif
        node=header.pNext;
    }
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice d,const VkDeviceCreateInfo *i,const VkAllocationCallbacks *a,VkDevice *out) {
    (void)d;(void)a;(void)out;device_calls++;
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
    int64_requested=i->pEnabledFeatures&&i->pEnabledFeatures->shaderInt64;
#endif
    for(const void *node=i->pNext;node;) {
        VkBaseInStructure header;memcpy(&header,node,sizeof(header));
        if(header.sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
            VkPhysicalDeviceVulkan13Features features;memcpy(&features,node,sizeof(features));
            requested=features.synchronization2;
        }
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
        if(header.sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            VkPhysicalDeviceVulkan12Features features;memcpy(&features,node,sizeof(features));
            address_requested=features.bufferDeviceAddress;
        }
#endif
        node=header.pNext;
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}
static void test_probe_explicitly_enables_supported_sync2(void) {
    supported=true;requested=false;device_calls=0;probe_t p={0};
    ASSERT_TRUE(!initialize_device(&p));ASSERT_EQ(device_calls,1u);ASSERT_TRUE(requested);PASS();
}
static void test_probe_rejects_missing_sync2_before_device_creation(void) {
    supported=false;requested=false;device_calls=0;probe_t p={0};
    ASSERT_TRUE(!initialize_device(&p));ASSERT_EQ(device_calls,0u);PASS();
}
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
static void test_address_probe_explicitly_enables_address_without_int64(void) {
    supported=true;address_supported=true;address_requested=false;device_calls=0;
    probe_t p={0};ASSERT_TRUE(!initialize_device(&p));
    ASSERT_EQ(device_calls,1u);ASSERT_TRUE(address_requested);ASSERT_TRUE(!int64_requested);PASS();
}
static void test_address_probe_rejects_missing_address_before_device_creation(void) {
    supported=true;address_supported=false;device_calls=0;probe_t p={0};
    ASSERT_TRUE(!initialize_device(&p));ASSERT_EQ(device_calls,0u);PASS();
}
#endif
int main(void) {
    RUN(test_probe_explicitly_enables_supported_sync2);
    RUN(test_probe_rejects_missing_sync2_before_device_creation);
#ifdef DC_PROBE_BUFFER_DEVICE_ADDRESS
    RUN(test_address_probe_explicitly_enables_address_without_int64);
    RUN(test_address_probe_rejects_missing_address_before_device_creation);
#endif
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
