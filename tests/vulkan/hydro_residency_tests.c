#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../src/vulkan/gpu_internal.h"

enum {
    SIDE = 256,
    PLANE_BYTES = SIDE * SIDE * sizeof(uint32_t),
    OCCUPANCY_PLANES = DC_HYDRO_MPM_OCCUPANCY_PLANE + 1u,
    MAX_PLANES = OCCUPANCY_PLANES + 2u
};
static int g_pass, g_fail;
static VkDeviceSize budget;
static uint32_t local_types, system_types, allocation_count, refusals;
static VkDeviceMemory allocations[MAX_PLANES];

#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { printf("FAIL %d: %s\n", __LINE__, #expr); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

void __real_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *);
void __wrap_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physical, VkPhysicalDeviceMemoryProperties *properties) {
    __real_vkGetPhysicalDeviceMemoryProperties(physical, properties);
    const VkMemoryPropertyFlags mapped = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    local_types = system_types = 0;
    for (uint32_t i = 0; i < properties->memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = properties->memoryTypes[i].propertyFlags;
        if ((flags & mapped) != mapped) continue;
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) local_types |= 1u << i;
        else system_types |= 1u << i;
    }
}

VkResult __real_vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *, VkDeviceMemory *);
VkResult __wrap_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *info,
                                  const VkAllocationCallbacks *callbacks, VkDeviceMemory *out) {
    /* Constrain the large mapped arrays on devices that also offer ordinary host RAM. */
    bool limited = system_types && (local_types & (1u << info->memoryTypeIndex)) && info->allocationSize >= PLANE_BYTES;
    if (limited && info->allocationSize > budget) {
        ++refusals;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    VkResult result = __real_vkAllocateMemory(device, info, callbacks, out);
    if (limited && result == VK_SUCCESS) {
        budget -= info->allocationSize;
        allocations[allocation_count++] = *out;
    }
    return result;
}

static bool reserved(VkDeviceMemory memory) {
    for (uint32_t i = 0; i < allocation_count; ++i)
        if (allocations[i] == memory) return true;
    return false;
}

static void check_residency(uint32_t planes, bool expect_occupancy, bool expect_velocity) {
    budget = (VkDeviceSize)planes * PLANE_BYTES;
    allocation_count = refusals = 0;
    dc_gpu_t *gpu = NULL;
    char err[256] = {0};
    if (!dc_gpu_create(&gpu, SIDE, SIDE, "build/shaders/pattern.comp.spv", err, sizeof(err))) {
        printf("FAIL GPU creation: %s\n", err);
        ++g_fail;
        return;
    }
    bool mapped = gpu->occupancy_mapped && gpu->velocity_mapped &&
                  gpu->hydro_addresses[DC_HYDRO_OCCUPANCY] && gpu->hydro_addresses[DC_HYDRO_VELOCITY];
    bool local_occupancy = reserved(gpu->occupancy_memory);
    bool local_velocity = reserved(gpu->velocity_memory);
    bool separate_types = local_types && system_types;
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(mapped);
    if (separate_types) {
        ASSERT_TRUE(refusals > 0);
        ASSERT_EQ(local_occupancy, expect_occupancy);
        ASSERT_EQ(local_velocity, expect_velocity);
    }
    PASS();
}

static void test_occupancy_receives_the_available_local_allocation(void) {
    check_residency(OCCUPANCY_PLANES, true, false);
}

static void test_velocity_receives_the_remaining_local_allocation(void) {
    check_residency(MAX_PLANES, true, true);
}

static void test_occupancy_remains_mapped_when_the_local_heap_is_full(void) {
    check_residency(0, false, false);
}

int main(void) {
    RUN(test_occupancy_receives_the_available_local_allocation);
    RUN(test_velocity_receives_the_remaining_local_allocation);
    RUN(test_occupancy_remains_mapped_when_the_local_heap_is_full);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
