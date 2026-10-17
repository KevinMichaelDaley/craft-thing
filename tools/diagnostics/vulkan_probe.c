#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>

enum { PROBE_WORDS = 16 };
static const uint64_t fence_deadline_ns = UINT64_C(1000000000);

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence fence;
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t *mapped;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor;
    VkPipelineLayout layout;
    VkShaderModule shader;
    VkPipeline pipeline;
} probe_t;

static int check(VkResult result, const char *stage) {
    printf("%s: %d\n", stage, result);
    fflush(stdout);
    return result == VK_SUCCESS;
}

static int submit(probe_t *p, const char *stage, int command) {
    printf("%s submission begin (fence deadline 1 second)\n", stage);
    fflush(stdout);
    VkSubmitInfo info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = command ? 1u : 0u,
                         .pCommandBuffers = command ? &p->command : NULL};
    if (!check(vkQueueSubmit(p->queue, 1, &info, p->fence), "queue submit"))
        return 0;
    if (!check(vkWaitForFences(p->device, 1, &p->fence, VK_TRUE,
                               fence_deadline_ns),
               stage))
        return 0;
    return check(vkResetFences(p->device, 1, &p->fence), "fence reset");
}

static int initialize_device(probe_t *p) {
    const char *layer = "VK_LAYER_KHRONOS_validation";
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                             .pApplicationName = "bounded device probe",
                             .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo instance = {.sType =
                                         VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &app};
    if (getenv("DC_VK_VALIDATE")) {
        instance.enabledLayerCount = 1;
        instance.ppEnabledLayerNames = &layer;
    }
    if (!check(vkCreateInstance(&instance, NULL, &p->instance),
               "instance create"))
        return 0;
    VkPhysicalDevice devices[16];
    uint32_t count = 16;
    if (!check(vkEnumeratePhysicalDevices(p->instance, &count, devices),
               "devices"))
        return 0;
    for (uint32_t i = 0; i < count && !p->physical; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        if (properties.vendorID != 0x8086 ||
            properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            continue;
        VkQueueFamilyProperties families[32];
        uint32_t family_count = 32;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count,
                                                 families);
        for (uint32_t family = 0; family < family_count; ++family)
            if (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                p->physical = devices[i];
                p->family = family;
                printf("device %s, vendor %04x, driver %u, API %u.%u.%u\n",
                       properties.deviceName, properties.vendorID,
                       properties.driverVersion,
                       VK_VERSION_MAJOR(properties.apiVersion),
                       VK_VERSION_MINOR(properties.apiVersion),
                       VK_VERSION_PATCH(properties.apiVersion));
                break;
            }
    }
    if (!p->physical) {
        puts("No Intel hardware compute device");
        return 0;
    }
    float priority = 1;
    VkDeviceQueueCreateInfo queue = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = p->family,
        .queueCount = 1,
        .pQueuePriorities = &priority};
    VkPhysicalDeviceVulkan13Features supported13={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 supported={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,.pNext=&supported13};
    vkGetPhysicalDeviceFeatures2(p->physical,&supported);
    if(!supported13.synchronization2) {
        puts("Device lacks required synchronization2");
        return 0;
    }
    VkPhysicalDeviceVulkan13Features features13={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,.synchronization2=VK_TRUE};
    VkDeviceCreateInfo device = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                 .pNext=&features13,
                                 .queueCreateInfoCount = 1,
                                 .pQueueCreateInfos = &queue};
    if (!check(vkCreateDevice(p->physical, &device, NULL, &p->device),
               "device create"))
        return 0;
    vkGetDeviceQueue(p->device, p->family, 0, &p->queue);
    return 1;
}

static int initialize(probe_t *p) {
    if (!initialize_device(p))
        return 0;
    VkFenceCreateInfo fence = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (!check(vkCreateFence(p->device, &fence, NULL, &p->fence),
               "fence create"))
        return 0;
    if (!submit(p, "empty queue fence", 0))
        return 0;
    VkCommandPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = p->family};
    if (!check(vkCreateCommandPool(p->device, &pool, NULL, &p->pool),
               "pool create"))
        return 0;
    VkCommandBufferAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = p->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};
    return check(vkAllocateCommandBuffers(p->device, &allocation, &p->command),
                 "command allocate");
}

static int allocate_buffer(probe_t *p) {
    VkBufferCreateInfo buffer = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                 .size = PROBE_WORDS * sizeof(uint32_t),
                                 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    if (!check(vkCreateBuffer(p->device, &buffer, NULL, &p->buffer),
               "buffer create"))
        return 0;
    VkMemoryRequirements requirements;
    VkPhysicalDeviceMemoryProperties properties;
    vkGetBufferMemoryRequirements(p->device, p->buffer, &requirements);
    vkGetPhysicalDeviceMemoryProperties(p->physical, &properties);
    uint32_t type = UINT32_MAX;
    const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            type = i;
            break;
        }
    if (type == UINT32_MAX)
        return 0;
    VkMemoryAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = type};
    return check(vkAllocateMemory(p->device, &allocation, NULL, &p->memory),
                 "memory allocate") &&
           check(vkBindBufferMemory(p->device, p->buffer, p->memory, 0),
                 "buffer bind") &&
           check(vkMapMemory(p->device, p->memory, 0, buffer.size, 0,
                             (void **)&p->mapped),
                 "memory map");
}

static int begin(probe_t *p) {
    VkCommandBufferBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    return check(vkResetCommandBuffer(p->command, 0), "command reset") &&
           check(vkBeginCommandBuffer(p->command, &info), "command begin");
}

static void host_barrier(probe_t *p, int compute) {
    VkMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                               .srcAccessMask =
                                   compute ? VK_ACCESS_SHADER_WRITE_BIT
                                           : VK_ACCESS_TRANSFER_WRITE_BIT,
                               .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(p->command,
                         compute ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                 : VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, NULL, 0,
                         NULL);
}

static int create_pipeline(probe_t *p, const char *path) {
    uint32_t code[16384];
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 0;
    }
    size_t bytes = fread(code, 1, sizeof(code), file);
    int end = fgetc(file);
    fclose(file);
    if (bytes < 4 || bytes % 4 || end != EOF || code[0] != 0x07230203u)
        return 0;
    VkShaderModuleCreateInfo shader = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = bytes,
        .pCode = code};
    if (!check(vkCreateShaderModule(p->device, &shader, NULL, &p->shader),
               "shader create"))
        return 0;
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo descriptor = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding};
    if (!check(vkCreateDescriptorSetLayout(p->device, &descriptor, NULL,
                                           &p->descriptor_layout),
               "descriptor layout"))
        return 0;
    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &p->descriptor_layout};
    if (!check(vkCreatePipelineLayout(p->device, &layout, NULL, &p->layout),
               "pipeline layout"))
        return 0;
    VkComputePipelineCreateInfo pipeline = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = p->shader,
                  .pName = "main"},
        .layout = p->layout};
    if (!check(vkCreateComputePipelines(p->device, VK_NULL_HANDLE, 1, &pipeline,
                                        NULL, &p->pipeline),
               "pipeline create"))
        return 0;
    VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &size};
    if (!check(
            vkCreateDescriptorPool(p->device, &pool, NULL, &p->descriptor_pool),
            "descriptor pool"))
        return 0;
    VkDescriptorSetAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = p->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &p->descriptor_layout};
    if (!check(vkAllocateDescriptorSets(p->device, &allocation, &p->descriptor),
               "descriptor allocate"))
        return 0;
    VkDescriptorBufferInfo buffer = {p->buffer, 0,
                                     PROBE_WORDS * sizeof(uint32_t)};
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = p->descriptor,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer};
    vkUpdateDescriptorSets(p->device, 1, &write, 0, NULL);
    return 1;
}

int main(void) {
    probe_t p = {0};
    setvbuf(stdout, NULL, _IOLBF, 0);
    puts("Bounded Intel hardware probe: empty queue, 16-word fill, "
         "16-invocation compute");
    if (!initialize(&p) || !allocate_buffer(&p) || !begin(&p))
        return 2;
    vkCmdFillBuffer(p.command, p.buffer, 0, PROBE_WORDS * sizeof(uint32_t),
                    0x2468ace0u);
    host_barrier(&p, 0);
    if (!check(vkEndCommandBuffer(p.command), "fill command end") ||
        !submit(&p, "fill fence", 1))
        return 2;
    for (uint32_t i = 0; i < PROBE_WORDS; ++i)
        if (p.mapped[i] != 0x2468ace0u)
            return 3;
    if (!create_pipeline(&p, "build/diagnostics/vulkan_probe.comp.spv") ||
        !begin(&p))
        return 2;
    vkCmdBindPipeline(p.command, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
    vkCmdBindDescriptorSets(p.command, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout,
                            0, 1, &p.descriptor, 0, NULL);
    vkCmdDispatch(p.command, 1, 1, 1);
    host_barrier(&p, 1);
    if (!check(vkEndCommandBuffer(p.command), "compute command end") ||
        !submit(&p, "compute fence", 1))
        return 2;
    for (uint32_t i = 0; i < PROBE_WORDS; ++i)
        if (p.mapped[i] != (0xcafe0000u ^ i))
            return 3;
    puts("PASS: all three fences completed and all 16 GPU words match");
    vkDestroyPipeline(p.device, p.pipeline, NULL);
    vkDestroyShaderModule(p.device, p.shader, NULL);
    vkDestroyPipelineLayout(p.device, p.layout, NULL);
    vkDestroyDescriptorPool(p.device, p.descriptor_pool, NULL);
    vkDestroyDescriptorSetLayout(p.device, p.descriptor_layout, NULL);
    vkUnmapMemory(p.device, p.memory);
    vkDestroyBuffer(p.device, p.buffer, NULL);
    vkFreeMemory(p.device, p.memory, NULL);
    vkDestroyFence(p.device, p.fence, NULL);
    vkDestroyCommandPool(p.device, p.pool, NULL);
    vkDestroyDevice(p.device, NULL);
    vkDestroyInstance(p.instance, NULL);
    return 0;
}
