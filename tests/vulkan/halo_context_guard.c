#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "dungeoncraft/gpu.h"

static dc_gpu_t *live_context;
static enum { NORMAL, ALLOCATE, BIND, MAP } failure;

VKAPI_ATTR VkResult VKAPI_CALL __real_vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo *,
    const VkAllocationCallbacks *, VkDeviceMemory *);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *info,
    const VkAllocationCallbacks *callbacks, VkDeviceMemory *memory) {
    if (failure == ALLOCATE) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return __real_vkAllocateMemory(device, info, callbacks, memory);
}

VKAPI_ATTR VkResult VKAPI_CALL __real_vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkBindBufferMemory(VkDevice device, VkBuffer buffer,
    VkDeviceMemory memory, VkDeviceSize offset) {
    if (failure == BIND) return VK_ERROR_OUT_OF_HOST_MEMORY;
    return __real_vkBindBufferMemory(device, buffer, memory, offset);
}

VKAPI_ATTR VkResult VKAPI_CALL __real_vkMapMemory(VkDevice, VkDeviceMemory, VkDeviceSize,
    VkDeviceSize, VkMemoryMapFlags, void **);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkMapMemory(VkDevice device, VkDeviceMemory memory,
    VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **mapped) {
    if (failure == MAP) return VK_ERROR_MEMORY_MAP_FAILED;
    return __real_vkMapMemory(device, memory, offset, size, flags, mapped);
}

bool halo_context_memory_errors(void) {
    static const char *const expected[] = {NULL, "vkAllocateMemory returned -2",
        "vkBindBufferMemory returned -1", "vkMapMemory returned -5"};
    bool passed = true;
    for (unsigned i = ALLOCATE; i <= MAP; ++i) {
        dc_gpu_t *gpu = NULL;
        char err[256] = {0};
        failure = i;
        bool created = dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err));
        failure = NORMAL;
        const char *details = strchr(err, '(');
        unsigned long long bytes = 0;
        unsigned type = VK_MAX_MEMORY_TYPES;
        if (created || gpu || !strstr(err, expected[i]) || !details ||
            sscanf(details, "(%llu bytes, memory type %u)", &bytes, &type) != 2 ||
            !bytes || type >= VK_MAX_MEMORY_TYPES) {
            fprintf(stderr, "Expected %s with allocation details, got [%s]\n", expected[i], err);
            passed = false;
        } else printf("Observed %s\n", err);
        dc_gpu_destroy(gpu);
    }
    return passed;
}

bool __real_dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                         const char *shader, char *err, uint32_t cap);
void __real_dc_gpu_destroy(dc_gpu_t *gpu);

/** Enforce a one-context budget for the fixture's independent comparison legs. */
bool __wrap_dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                         const char *shader, char *err, uint32_t cap) {
    if (live_context) {
        if (out) *out = NULL;
        if (err && cap) snprintf(err, cap, "Halo fixture already owns a Vulkan context");
        return false;
    }
    if (!__real_dc_gpu_create(out, width, height, shader, err, cap)) return false;
    live_context = *out;
    return true;
}

/** Track only completed allocations returned to a fixture. */
void __wrap_dc_gpu_destroy(dc_gpu_t *gpu) {
    if (gpu == live_context) live_context = NULL;
    __real_dc_gpu_destroy(gpu);
}
