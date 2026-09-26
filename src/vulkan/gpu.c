#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_vulkan.h>
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
    SDL_Window *window;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkImage *swap_images;
    uint32_t swap_count;
    VkExtent2D swap_extent;
    VkImage frame_image;
    VkDeviceMemory frame_memory;
    VkSemaphore acquire_sem;
    VkSemaphore present_sem;
    uint32_t width, height;
};

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

void dc_gpu_destroy(dc_gpu_t *gpu) {
    if (!gpu) return;
    if (gpu->device) vkDeviceWaitIdle(gpu->device);
    if (gpu->acquire_sem) vkDestroySemaphore(gpu->device, gpu->acquire_sem, NULL);
    if (gpu->present_sem) vkDestroySemaphore(gpu->device, gpu->present_sem, NULL);
    if (gpu->frame_image) vkDestroyImage(gpu->device, gpu->frame_image, NULL);
    if (gpu->frame_memory) vkFreeMemory(gpu->device, gpu->frame_memory, NULL);
    if (gpu->swapchain) vkDestroySwapchainKHR(gpu->device, gpu->swapchain, NULL);
    free(gpu->swap_images);
    if (gpu->mapped) vkUnmapMemory(gpu->device, gpu->memory);
    if (gpu->command_pool) vkDestroyCommandPool(gpu->device, gpu->command_pool, NULL);
    if (gpu->pipeline) vkDestroyPipeline(gpu->device, gpu->pipeline, NULL);
    if (gpu->pipeline_layout) vkDestroyPipelineLayout(gpu->device, gpu->pipeline_layout, NULL);
    if (gpu->descriptor_pool) vkDestroyDescriptorPool(gpu->device, gpu->descriptor_pool, NULL);
    if (gpu->set_layout) vkDestroyDescriptorSetLayout(gpu->device, gpu->set_layout, NULL);
    if (gpu->cells) vkDestroyBuffer(gpu->device, gpu->cells, NULL);
    if (gpu->memory) vkFreeMemory(gpu->device, gpu->memory, NULL);
    if (gpu->device) vkDestroyDevice(gpu->device, NULL);
    if (gpu->surface) vkDestroySurfaceKHR(gpu->instance, gpu->surface, NULL);
    if (gpu->instance) vkDestroyInstance(gpu->instance, NULL);
    if (gpu->window) { SDL_DestroyWindow(gpu->window); SDL_QuitSubSystem(SDL_INIT_VIDEO); }
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
            VkBool32 can_present = VK_FALSE;
            if (gpu->surface) vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], i, gpu->surface, &can_present);
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                (!gpu->surface || can_present)) {
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

static bool make_presentation(dc_gpu_t *gpu, uint32_t requested_width,
                              uint32_t requested_height, char *err, uint32_t cap) {
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->physical, gpu->surface, &caps) != VK_SUCCESS)
        return error(err, cap, "Cannot query Vulkan surface capabilities");
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return error(err, cap, "Vulkan surface does not support transfer destination images");
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, gpu->surface, &format_count, NULL);
    if (!format_count) return error(err, cap, "Vulkan surface has no formats");
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (!formats) return error(err, cap, "Out of memory listing surface formats");
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->physical, gpu->surface, &format_count, formats);
    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) { chosen = formats[i]; break; }
    }
    free(formats);
    VkFormatProperties frame_props, swap_props;
    vkGetPhysicalDeviceFormatProperties(gpu->physical, VK_FORMAT_R8G8B8A8_UNORM, &frame_props);
    vkGetPhysicalDeviceFormatProperties(gpu->physical, chosen.format, &swap_props);
    if (!(frame_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
        !(swap_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        return error(err, cap, "Vulkan image formats do not support blit presentation");
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = requested_width;
        extent.height = requested_height;
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }
    gpu->swap_extent = extent;
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount && image_count > caps.maxImageCount) image_count = caps.maxImageCount;
    VkSwapchainCreateInfoKHR swap_info = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = gpu->surface, .minImageCount = image_count,
        .imageFormat = chosen.format, .imageColorSpace = chosen.colorSpace,
        .imageExtent = extent, .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE };
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) {
        for (uint32_t bit = 0; bit < 4; ++bit) {
            VkCompositeAlphaFlagBitsKHR alpha = (VkCompositeAlphaFlagBitsKHR)(1u << bit);
            if (caps.supportedCompositeAlpha & alpha) { swap_info.compositeAlpha = alpha; break; }
        }
    }
    if (vkCreateSwapchainKHR(gpu->device, &swap_info, NULL, &gpu->swapchain) != VK_SUCCESS)
        return error(err, cap, "Cannot create Vulkan swapchain");
    vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &gpu->swap_count, NULL);
    gpu->swap_images = calloc(gpu->swap_count, sizeof(*gpu->swap_images));
    if (!gpu->swap_images) return error(err, cap, "Out of memory listing swapchain images");
    vkGetSwapchainImagesKHR(gpu->device, gpu->swapchain, &gpu->swap_count, gpu->swap_images);
    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { gpu->width, gpu->height, 1 }, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
    if (vkCreateImage(gpu->device, &image_info, NULL, &gpu->frame_image) != VK_SUCCESS)
        return error(err, cap, "Cannot create cell presentation image");
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu->device, gpu->frame_image, &req);
    VkPhysicalDeviceMemoryProperties memory_props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &memory_props);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < memory_props.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (memory_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            type = i; break;
        }
    }
    if (type == UINT32_MAX) return error(err, cap, "No device-local image memory");
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    if (vkAllocateMemory(gpu->device, &alloc, NULL, &gpu->frame_memory) != VK_SUCCESS ||
        vkBindImageMemory(gpu->device, gpu->frame_image, gpu->frame_memory, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate presentation image memory");
    VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkCreateSemaphore(gpu->device, &semaphore_info, NULL, &gpu->acquire_sem) != VK_SUCCESS ||
        vkCreateSemaphore(gpu->device, &semaphore_info, NULL, &gpu->present_sem) != VK_SUCCESS)
        return error(err, cap, "Cannot create present semaphores");
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
    VkPushConstantRange range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 28 };
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

static bool create_gpu(dc_gpu_t **out, uint32_t width, uint32_t height,
                       uint32_t window_width, uint32_t window_height,
                       const char *shader_path, char *err, uint32_t cap) {
    if (out) *out = NULL;
    if (!out || !width || !height || !shader_path ||
        (uint64_t)width * height > UINT32_MAX / sizeof(uint32_t))
        return error(err, cap, "Invalid GPU grid dimensions or arguments");
    dc_gpu_t *gpu = calloc(1, sizeof(*gpu));
    if (!gpu) return error(err, cap, "Out of memory creating GPU context");
    gpu->width = width; gpu->height = height;
    if (window_width && window_height) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            error(err, cap, SDL_GetError()); goto fail;
        }
        gpu->window = SDL_CreateWindow("Dungeoncraft GPU testbed", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, (int)window_width, (int)window_height, SDL_WINDOW_VULKAN);
        if (!gpu->window) { error(err, cap, SDL_GetError()); SDL_QuitSubSystem(SDL_INIT_VIDEO); goto fail; }
    }
    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "dungeoncraft", .apiVersion = VK_API_VERSION_1_3 };
    VkInstanceCreateInfo instance_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app };
    unsigned int extension_count = 0;
    const char **extensions = NULL;
    if (gpu->window) {
        if (!SDL_Vulkan_GetInstanceExtensions(gpu->window, &extension_count, NULL)) {
            error(err, cap, SDL_GetError()); goto fail;
        }
        extensions = calloc(extension_count, sizeof(*extensions));
        if (!extensions || !SDL_Vulkan_GetInstanceExtensions(gpu->window, &extension_count, extensions)) {
            free(extensions); error(err, cap, "Cannot query SDL Vulkan extensions"); goto fail;
        }
        instance_info.enabledExtensionCount = extension_count;
        instance_info.ppEnabledExtensionNames = extensions;
    }
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
    VkResult instance_result = vkCreateInstance(&instance_info, NULL, &gpu->instance);
    free(extensions);
    if (instance_result != VK_SUCCESS) {
        error(err, cap, "Cannot create Vulkan 1.3 instance"); goto fail;
    }
    if (gpu->window && !SDL_Vulkan_CreateSurface(gpu->window, gpu->instance, &gpu->surface)) {
        error(err, cap, SDL_GetError()); goto fail;
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
    const char *swap_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (gpu->window) {
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = &swap_extension;
    }
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
    if (gpu->window && !make_presentation(gpu, window_width, window_height, err, cap)) goto fail;
    *out = gpu;
    return true;
fail:
    dc_gpu_destroy(gpu);
    return false;
}

bool dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                   const char *shader_path, char *err, uint32_t cap) {
    return create_gpu(out, width, height, 0, 0, shader_path, err, cap);
}

bool dc_gpu_create_window(dc_gpu_t **out, uint32_t width, uint32_t height,
                          uint32_t window_width, uint32_t window_height,
                          const char *shader_path, char *err, uint32_t cap) {
    if (!window_width || !window_height)
        return error(err, cap, "Invalid Vulkan window dimensions");
    return create_gpu(out, width, height, window_width, window_height, shader_path, err, cap);
}

static bool dispatch_cells(dc_gpu_t *gpu, const uint32_t push[7], char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset compute command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin compute command buffer");
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 28, push);
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

bool dc_gpu_pattern(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    uint32_t push[7] = { gpu->width, gpu->height, 0, 0, 0, 0, 0 };
    return dispatch_cells(gpu, push, err, cap);
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
    if (!gpu) return error(err, cap, "GPU context is null");
    uint32_t push[7] = { gpu->width, gpu->height, 1, x, y, radius, rgba };
    return dispatch_cells(gpu, push, err, cap);
}

static void image_barrier(VkCommandBuffer command, VkImage image,
                          VkImageLayout old_layout, VkImageLayout new_layout,
                          VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                          VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage, .srcAccessMask = src_access,
        .dstStageMask = dst_stage, .dstAccessMask = dst_access,
        .oldLayout = old_layout, .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
    vkCmdPipelineBarrier2(command, &dependency);
}

bool dc_gpu_present(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu || !gpu->swapchain) return error(err, cap, "GPU window is not initialized");
    uint32_t index = 0;
    VkResult result = vkAcquireNextImageKHR(gpu->device, gpu->swapchain, UINT64_MAX,
        gpu->acquire_sem, VK_NULL_HANDLE, &index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Cannot acquire Vulkan window image");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset present command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin present command buffer");
    VkBufferMemoryBarrier2 buffer_barrier = { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = gpu->cells, .offset = 0, .size = VK_WHOLE_SIZE };
    VkDependencyInfo buffer_dep = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &buffer_barrier };
    vkCmdPipelineBarrier2(gpu->command, &buffer_dep);
    image_barrier(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy = { .bufferOffset = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { gpu->width, gpu->height, 1 } };
    vkCmdCopyBufferToImage(gpu->command, gpu->cells, gpu->frame_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    image_barrier(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT);
    image_barrier(gpu->command, gpu->swap_images[index], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkImageBlit blit = { .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { {0, 0, 0}, {(int32_t)gpu->width, (int32_t)gpu->height, 1} },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { {0, 0, 0}, {(int32_t)gpu->swap_extent.width, (int32_t)gpu->swap_extent.height, 1} } };
    vkCmdBlitImage(gpu->command, gpu->frame_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    image_barrier(gpu->command, gpu->swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end present command buffer");
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &gpu->present_sem };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        return error(err, cap, "Vulkan present submission failed");
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &gpu->present_sem,
        .swapchainCount = 1, .pSwapchains = &gpu->swapchain, .pImageIndices = &index };
    result = vkQueuePresentKHR(gpu->queue, &present);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        return error(err, cap, "Vulkan swapchain presentation failed");
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Vulkan present wait failed");
    return true;
}
