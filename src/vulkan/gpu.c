#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "gpu_internal.h"

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
    if (gpu->device) dc_gpu_rigid_destroy(gpu);
    if (gpu->device) dc_gpu_chunks_destroy(gpu);
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

static bool make_cells(dc_gpu_t *gpu, VkDeviceSize bytes, char *err, uint32_t cap) {
    return dc_gpu_make_mapped_buffer(gpu, bytes, &gpu->cells,
                                     &gpu->memory, &gpu->mapped, err, cap);
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
    VkDescriptorSetLayoutBinding bindings[32] = {0};
    for (uint32_t i = 0; i < 32; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 32, .pBindings = bindings };
    if (vkCreateDescriptorSetLayout(gpu->device, &layout_info, NULL, &gpu->set_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create descriptor layout");
    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32 };
    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &size };
    if (vkCreateDescriptorPool(gpu->device, &pool_info, NULL, &gpu->descriptor_pool) != VK_SUCCESS)
        return error(err, cap, "Cannot create descriptor pool");
    VkDescriptorSetAllocateInfo set_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gpu->descriptor_pool, .descriptorSetCount = 1,
        .pSetLayouts = &gpu->set_layout };
    if (vkAllocateDescriptorSets(gpu->device, &set_info, &gpu->descriptor) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate descriptor set");
    VkDescriptorBufferInfo buffers[32] = {
        { gpu->cells, 0, bytes },
        { gpu->chunk_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS * sizeof(dc_cell_t) },
        { gpu->page_buffer, 0, (VkDeviceSize)gpu->page_width * gpu->page_height * sizeof(uint32_t) },
        { gpu->occupancy_buffer, 0, bytes * 2 },
        { gpu->body_buffer, 0, dc_gpu_body_storage_bytes(gpu) },
        { gpu->trace_buffer, 0, 3 * sizeof(uint32_t) },
        { gpu->halo_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_GPU_HALO_CELLS * sizeof(dc_gpu_halo_cell_t) },
        { gpu->transfer_buffer, 0, sizeof(dc_gpu_transfer_t) },
        { gpu->fluid_a_buffer, 0, bytes },
        { gpu->fluid_b_buffer, 0, bytes },
        { gpu->velocity_buffer, 0, bytes * 2 },
        { gpu->pressure_a_buffer, 0, bytes },
        { gpu->marker_a_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t) },
        { gpu->marker_b_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t) },
        { gpu->marker_count_a_buffer, 0, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t) },
        { gpu->marker_count_b_buffer, 0, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t) },
        { gpu->marker_grid_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS * sizeof(uint32_t) },
        { gpu->slot_page_buffer, 0, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t) },
        { gpu->slot_seed_buffer, 0, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t) },
        { gpu->particle_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t) },
        { gpu->particle_count_buffer, 0, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t) },
        { gpu->mpm_proposal_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t) },
        { gpu->mpm_output_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t) },
        { gpu->mpm_grid_buffer, 0, bytes * 4 },
        { gpu->mpm_force_buffer, 0, bytes * 4 },
        { gpu->mpm_velocity_buffer, 0, bytes * 2 },
        { gpu->mpm_accept_buffer, 0, (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK * sizeof(uint32_t) },
        { gpu->mpm_activity_buffer, 0, (VkDeviceSize)(7u + 2u * ((gpu->width + 15u) / 16u) * ((gpu->height + 15u) / 16u)) * sizeof(uint32_t) },
        { gpu->mpm_label_a_buffer, 0, bytes },
        { gpu->mpm_label_b_buffer, 0, bytes },
        { gpu->mpm_component_size_buffer, 0, bytes },
        { gpu->fluid_previous_buffer, 0, bytes }
    };
    VkWriteDescriptorSet writes[32] = {0};
    for (uint32_t i = 0; i < 32; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = gpu->descriptor;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffers[i];
    }
    vkUpdateDescriptorSets(gpu->device, 32, writes, 0, NULL);
    VkPushConstantRange range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = 32 };
    VkPipelineLayoutCreateInfo pipeline_layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &gpu->set_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &range };
    if (vkCreatePipelineLayout(gpu->device, &pipeline_layout_info, NULL, &gpu->pipeline_layout) != VK_SUCCESS)
        return error(err, cap, "Cannot create compute pipeline layout");
    VkShaderModule shader = VK_NULL_HANDLE;
    if (!dc_gpu_load_shader_module(gpu, path, &shader, err, cap)) return false;
    VkComputePipelineCreateInfo pipeline_info = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main" },
        .layout = gpu->pipeline_layout };
    VkResult result = vkCreateComputePipelines(gpu->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &gpu->pipeline);
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
    gpu->view_width = width; gpu->view_height = height;
    gpu->display_zoom = 1;
    gpu->fluid_interval = 1;
    gpu->fluid_step_scale = 1.0f;
    for (uint32_t i = 0; i < DC_GPU_CHUNK_SLOTS; ++i) gpu->slot_page[i] = UINT32_MAX;
    if (window_width && window_height) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            error(err, cap, SDL_GetError()); goto fail;
        }
        uint32_t flags = SDL_WINDOW_VULKAN;
#ifdef DC_NATIVE_VIEW
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
        gpu->window = SDL_CreateWindow("Dungeoncraft GPU testbed", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, (int)window_width, (int)window_height, flags);
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
        if (!layers) { free(extensions); error(err, cap, "Cannot list Vulkan validation layers"); goto fail; }
        vkEnumerateInstanceLayerProperties(&layer_count, layers);
        bool present = false;
        for (uint32_t i = 0; i < layer_count; ++i)
            if (strcmp(layers[i].layerName, layer) == 0) present = true;
        free(layers);
        if (!present) { free(extensions); error(err, cap, "VK_LAYER_KHRONOS_validation is not installed"); goto fail; }
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
    VkDeviceSize atlas_bytes = (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS * sizeof(dc_cell_t);
    if (!dc_gpu_pick_device(gpu, bytes > atlas_bytes ? bytes : atlas_bytes, err, cap)) goto fail;
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
    if (!make_cells(gpu, bytes, err, cap) || !dc_gpu_chunks_init(gpu, err, cap) ||
        !dc_gpu_rigid_buffers_init(gpu, err, cap) ||
        !make_pipeline(gpu, shader_path, bytes, err, cap)) goto fail;
    if (!dc_gpu_rigid_pipeline_init(gpu, "build/shaders/rigid.comp.spv", err, cap) ||
        !dc_gpu_broadphase_pipeline_init(gpu, err, cap) ||
        !dc_gpu_tick_init(gpu, err, cap) ||
        !dc_gpu_halo_pipeline_init(gpu, err, cap)) goto fail;
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

bool dc_gpu_set_viewport(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                         uint32_t width, uint32_t height) {
    if (!gpu || !width || !height || x >= gpu->width || y >= gpu->height ||
        width > gpu->width - x || height > gpu->height - y ||
        (gpu->swapchain &&
         ((uint64_t)width * gpu->display_zoom > gpu->swap_extent.width ||
          (uint64_t)height * gpu->display_zoom > gpu->swap_extent.height))) return false;
    gpu->view_x = x;
    gpu->view_y = y;
    gpu->view_width = width;
    gpu->view_height = height;
    return true;
}

bool dc_gpu_set_display_zoom(dc_gpu_t *gpu, uint32_t zoom) {
    if (!gpu || !gpu->swapchain || (zoom != 1u && zoom != 2u && zoom != 4u) ||
        (uint64_t)gpu->view_width * zoom > gpu->swap_extent.width ||
        (uint64_t)gpu->view_height * zoom > gpu->swap_extent.height) return false;
    gpu->display_zoom = zoom;
    return true;
}

bool dc_gpu_screen_cell(dc_gpu_t *gpu, uint32_t screen_x, uint32_t screen_y,
                        uint32_t *cell_x, uint32_t *cell_y) {
    if (!gpu || !gpu->swapchain || !cell_x || !cell_y) return false;
    uint32_t image_width = gpu->view_width * gpu->display_zoom;
    uint32_t image_height = gpu->view_height * gpu->display_zoom;
    uint32_t left = (gpu->swap_extent.width - image_width) / 2u;
    uint32_t top = (gpu->swap_extent.height - image_height) / 2u;
    if (screen_x < left || screen_y < top || screen_x - left >= image_width ||
        screen_y - top >= image_height) return false;
    *cell_x = (screen_x - left) / gpu->display_zoom;
    *cell_y = (screen_y - top) / gpu->display_zoom;
    return true;
}

bool dc_gpu_set_overlay(dc_gpu_t *gpu, dc_gpu_overlay_t overlay) {
    if (!gpu || overlay < DC_GPU_OVERLAY_NONE ||
        overlay > DC_GPU_OVERLAY_STAGES) return false;
    gpu->overlay = overlay;
    return true;
}

bool dc_gpu_set_window_title(dc_gpu_t *gpu, const char *title) {
    if (!gpu || !gpu->window || !title) return false;
    SDL_SetWindowTitle(gpu->window, title);
    return true;
}

static bool dispatch_cells(dc_gpu_t *gpu, const uint32_t push[7], char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset compute command buffer");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin compute command buffer");
    VkMemoryBarrier2 upload_barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo upload_dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &upload_barrier };
    vkCmdPipelineBarrier2(gpu->command, &upload_dependency);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 28, push);
    uint32_t dispatch_width = push[2] == 2u ? gpu->view_width : gpu->width;
    uint32_t dispatch_height = push[2] == 2u ? gpu->view_height : gpu->height;
    vkCmdDispatch(gpu->command, (dispatch_width + 15u) / 16u,
                  (dispatch_height + 15u) / 16u, 1);
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

bool dc_gpu_paint(dc_gpu_t *gpu, uint32_t x, uint32_t y, uint32_t radius,
                  uint32_t rgba, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    uint32_t push[7] = { gpu->width, gpu->height, 1, x, y, radius, rgba };
    return dispatch_cells(gpu, push, err, cap);
}

bool dc_gpu_render_chunks(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    uint32_t push[7] = { gpu->width, gpu->height, 2,
                         dc_gpu_render_flags(gpu), (uint32_t)gpu->overlay,
                         gpu->view_x, gpu->view_y };
    return dispatch_cells(gpu, push, err, cap);
}

bool dc_gpu_paint_material(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                           uint32_t radius, uint16_t material,
                           char *err, uint32_t cap) {
    if (!gpu) return error(err, cap, "GPU context is null");
    uint32_t push[7] = { gpu->width, gpu->height, 3, x, y, radius, material };
    return dispatch_cells(gpu, push, err, cap);
}

bool dc_gpu_set_tick_water_source(dc_gpu_t *gpu, bool enabled,
                                  uint32_t x, uint32_t y) {
    if (!gpu || (enabled && (x >= gpu->width || y >= gpu->height))) return false;
    gpu->tick_water_source = enabled;
    gpu->tick_water_x = x;
    gpu->tick_water_y = y;
    return true;
}

bool dc_gpu_memory_stats(const dc_gpu_t *gpu, dc_gpu_memory_stats_t *stats) {
    if (!gpu || !stats) return false;
    stats->mapped_local_bytes = gpu->mapped_local_bytes;
    stats->mapped_system_bytes = gpu->mapped_system_bytes;
    stats->device_only_bytes = gpu->device_only_bytes;
    return true;
}

void dc_gpu_record_tick_water_source(dc_gpu_t *gpu) {
    if (!gpu->tick_water_source) return;
    VkMemoryBarrier2 upload = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &upload };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    vkCmdBindPipeline(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE, gpu->pipeline);
    vkCmdBindDescriptorSets(gpu->command, VK_PIPELINE_BIND_POINT_COMPUTE,
        gpu->pipeline_layout, 0, 1, &gpu->descriptor, 0, NULL);
    uint32_t push[7] = { gpu->width, gpu->height, 4,
                          gpu->tick_water_x, gpu->tick_water_y, 2, DC_MATERIAL_WATER };
    vkCmdPushConstants(gpu->command, gpu->pipeline_layout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
    vkCmdDispatch(gpu->command, 1u, 1u, 1u);
    VkMemoryBarrier2 finish = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT };
    dependency.pMemoryBarriers = &finish;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
}
