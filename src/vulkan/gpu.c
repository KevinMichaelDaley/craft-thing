#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

#include "dungeoncraft/gpu.h"

struct dc_gpu {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    VkBuffer cells;
    VkDeviceMemory memory;
    void *mapped;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer command;
    uint32_t width, height;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

void dc_gpu_destroy(dc_gpu_t *gpu) {
    if (!gpu) return;
    if (gpu->device) vkDeviceWaitIdle(gpu->device);
    if (gpu->mapped) vkUnmapMemory(gpu->device, gpu->memory);
    if (gpu->command_pool) vkDestroyCommandPool(gpu->device, gpu->command_pool, NULL);
    if (gpu->pipeline) vkDestroyPipeline(gpu->device, gpu->pipeline, NULL);
    if (gpu->pipeline_layout) vkDestroyPipelineLayout(gpu->device, gpu->pipeline_layout, NULL);
    if (gpu->descriptor_pool) vkDestroyDescriptorPool(gpu->device, gpu->descriptor_pool, NULL);
    if (gpu->set_layout) vkDestroyDescriptorSetLayout(gpu->device, gpu->set_layout, NULL);
    if (gpu->cells) vkDestroyBuffer(gpu->device, gpu->cells, NULL);
    if (gpu->memory) vkFreeMemory(gpu->device, gpu->memory, NULL);
    if (gpu->device) vkDestroyDevice(gpu->device, NULL);
    if (gpu->instance) vkDestroyInstance(gpu->instance, NULL);
    free(gpu);
}

static bool pick_device(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap) {
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(gpu->instance, &count, NULL) != VK_SUCCESS || !count)
        return error(err, cap, "No Vulkan device found");
    VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
    if (!devices) return error(err, cap, "Out of memory listing Vulkan devices");
    VkResult result = vkEnumeratePhysicalDevices(gpu->instance, &count, devices);
    for (uint32_t d = 0; d < count && result == VK_SUCCESS && !gpu->physical; ++d) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[d], &props);
        if (props.apiVersion < VK_API_VERSION_1_3 || props.limits.maxStorageBufferRange < bytes) continue;
        uint32_t n = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &n, NULL);
        VkQueueFamilyProperties *families = calloc(n, sizeof(*families));
        if (!families) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &n, families);
        for (uint32_t i = 0; i < n; ++i) {
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                gpu->physical = devices[d]; gpu->family = i; break;
            }
        }
        free(families);
    }
    free(devices);
    if (!gpu->physical) return error(err, cap, "No Vulkan 1.3 compute device supports this grid");
    return true;
}

static bool make_cells(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(gpu->device, &info, NULL, &gpu->cells) != VK_SUCCESS)
        return error(err, cap, "Cannot create cell buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu->device, gpu->cells, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &props);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((req.memoryTypeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & flags) == flags) { type = i; break; }
    }
    if (type == UINT32_MAX) return error(err, cap, "No host-visible coherent cell memory");
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    if (vkAllocateMemory(gpu->device, &alloc, NULL, &gpu->memory) != VK_SUCCESS ||
        vkBindBufferMemory(gpu->device, gpu->cells, gpu->memory, 0) != VK_SUCCESS ||
        vkMapMemory(gpu->device, gpu->memory, 0, bytes, 0, &gpu->mapped) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate cell memory");
    return true;
}

static bool make_pipeline(dc_gpu_t *gpu, const char *path, VkDeviceSize bytes,
                          char *err, uint32_t cap) {
    VkDescriptorSetLayoutBinding binding = { .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT };
    VkDescriptorSetLayoutCreateInfo layout_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &binding };
    if (vkCreateDescriptorSetLayout(gpu->device, &layout_info, NULL, &gpu->set_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create descriptor layout");
    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &size };
    if (vkCreateDescriptorPool(gpu->device, &pool_info, NULL, &gpu->descriptor_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create descriptor pool");
    VkDescriptorSetAllocateInfo set_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gpu->descriptor_pool, .descriptorSetCount = 1,
        .pSetLayouts = &gpu->set_layout };
    if (vkAllocateDescriptorSets(gpu->device, &set_info, &gpu->descriptor) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate descriptor set");
    VkDescriptorBufferInfo buffer_info = { .buffer = gpu->cells, .offset = 0, .range = bytes };
    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = gpu->descriptor, .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_info };
    vkUpdateDescriptorSets(gpu->device, 1, &write, 0, NULL);
    VkPushConstantRange range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 8 };
    VkPipelineLayoutCreateInfo pipeline_layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &gpu->set_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &range };
    if (vkCreatePipelineLayout(gpu->device, &pipeline_layout_info, NULL, &gpu->pipeline_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create compute pipeline layout");
    FILE *file = fopen(path, "rb");
    if (!file) return error(err, cap, "Cannot open SPIR-V shader; run make shaders");
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return error(err, cap, "Cannot size SPIR-V shader"); }
    long length = ftell(file);
    if (length < 4 || (length & 3) || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return error(err, cap, "Invalid SPIR-V shader size");
    }
    uint32_t *words = malloc((size_t)length);
    if (!words) { fclose(file); return error(err, cap, "Out of memory reading shader"); }
    bool read_ok = fread(words, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!read_ok) { free(words); return error(err, cap, "Cannot read SPIR-V shader"); }
    VkShaderModuleCreateInfo shader_info = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)length, .pCode = words };
    VkShaderModule shader = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(gpu->device, &shader_info, NULL, &shader);
    free(words);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create SPIR-V shader module");
    VkComputePipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &gpu->pipeline);
    vkDestroyShaderModule(gpu->device, shader, NULL);
    if (result != VK_SUCCESS) return error(err, cap, "Cannot create compute pipeline");
    return true;
}

bool dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                   const char *shader_path, char *err, uint32_t cap) {
    if (out) *out = NULL;
    if (!out || !width || !height || !shader_path ||
        (uint64_t)width * height > UINT32_MAX / sizeof(uint32_t))
        return error(err, cap, "Invalid GPU grid dimensions or arguments");
    dc_gpu_t *gpu = calloc(1, sizeof(*gpu));
    if (!gpu) return error(err, cap, "Out of memory creating GPU context");
    gpu->width = width; gpu->height = height;
    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "dungeoncraft", .apiVersion = VK_API_VERSION_1_3 };
    VkInstanceCreateInfo instance_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app };
    const char *validation = getenv("DC_VK_VALIDATE");
    const char *layer = "VK_LAYER_KHRONOS_validation";
    if (validation && validation[0] == '1') {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties *layers = calloc(layer_count, sizeof(*layers));
        if (!layers) { error(err, cap, "Cannot list Vulkan validation layers"); goto fail; }
        vkEnumerateInstanceLayerProperties(&layer_count, layers);
        bool present = false;
        for (uint32_t i = 0; i < layer_count; ++i)
            if (strcmp(layers[i].layerName, layer) == 0) present = true;
        free(layers);
        if (!present) { error(err, cap, "VK_LAYER_KHRONOS_validation is not installed"); goto fail; }
        instance_info.enabledLayerCount = 1;
        instance_info.ppEnabledLayerNames = &layer;
    }
    if (vkCreateInstance(&instance_info, NULL, &gpu->instance) != VK_SUCCESS) {
        error(err, cap, "Cannot create Vulkan 1.3 instance"); goto fail;
    }
    VkDeviceSize bytes = (VkDeviceSize)width * height * 4;
    if (!pick_device(gpu, bytes, err, cap)) goto fail;
    VkPhysicalDeviceSynchronization2Features sync = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
    VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &sync };
    vkGetPhysicalDeviceFeatures2(gpu->physical, &features);
    if (!sync.synchronization2) { error(err, cap, "Device lacks synchronization2"); goto fail; }
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = gpu->family, .queueCount = 1, .pQueuePriorities = &priority };
    VkDeviceCreateInfo device_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &sync, .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info };
    if (vkCreateDevice(gpu->physical, &device_info, NULL, &gpu->device) != VK_SUCCESS) {
        error(err, cap, "Cannot create Vulkan compute device"); goto fail;
    }
    vkGetDeviceQueue(gpu->device, gpu->family, 0, &gpu->queue);
    if (!make_cells(gpu, bytes, err, cap) || !make_pipeline(gpu, shader_path, bytes, err, cap)) goto fail;
    VkCommandPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = gpu->family };
    if (vkCreateCommandPool(gpu->device, &pool_info, NULL, &gpu->command_pool) != VK_SUCCESS) {
        error(err, cap, "Cannot create compute command pool"); goto fail;
    }
    VkCommandBufferAllocateInfo command_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = gpu->command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1 };
    if (vkAllocateCommandBuffers(gpu->device, &command_info, &gpu->command) != VK_SUCCESS) {
        error(err, cap, "Cannot allocate compute command buffer"); goto fail;
    }
    *out = gpu;
    return true;
fail:
    dc_gpu_destroy(gpu);
    return false;
}

bool dc_gpu_pattern(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset compute command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin compute command buffer");
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[2] = { gpu->width, gpu->height };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, push);
    vkCmdDispatch(gpu->command, (gpu->width + 15u) / 16u, (gpu->height + 15u) / 16u, 1);
    VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end compute command buffer");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Compute submission failed");
    return true;
}

bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err, uint32_t cap) {
    if (!gpu || !cells || cell_count < (uint64_t)gpu->width * gpu->height)
        return error(err, cap, "Readback buffer is too small");
    memcpy(cells, gpu->mapped, (size_t)gpu->width * gpu->height * sizeof(uint32_t));
    return true;
}

bool dc_gpu_paint(dc_gpu_t *gpu, uint32_t x, uint32_t y, uint32_t radius,
                  uint32_t rgba, char *err, uint32_t cap) {
    (void)gpu; (void)x; (void)y; (void)radius; (void)rgba;
    return error(err, cap, "GPU brush is not implemented");
}
