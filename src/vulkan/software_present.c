#include <stdlib.h>
#include <string.h>

#include "software_present.h"

#if defined(DC_HAVE_X11) && defined(SDL_VIDEO_DRIVER_X11)
#include <SDL_syswm.h>
#include <sys/socket.h>
#endif

static bool use_local_presenter(dc_gpu_t *gpu) {
#if defined(DC_HAVE_X11) && defined(SDL_VIDEO_DRIVER_X11)
    const char *setting = getenv("DC_X11_SOFTWARE_PRESENT");
    if (setting && !strcmp(setting, "0")) return false;
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(gpu->window, &wm) || wm.subsystem != SDL_SYSWM_X11) return false;
    Display *display = wm.info.x11.display;
    struct sockaddr_storage peer;
    socklen_t size = sizeof(peer);
    if (getpeername(ConnectionNumber(display), (struct sockaddr *)&peer, &size) ||
        peer.ss_family != AF_UNIX) return false;
    if (setting && !strcmp(setting, "1")) return true;
    int opcode, event, error;
    return gpu->vendor_id == 0x10de &&
        XQueryExtension(display, "MIT-SHM", &opcode, &event, &error) &&
        !XQueryExtension(display, "DRI3", &opcode, &event, &error) &&
        !XQueryExtension(display, "NV-GLX", &opcode, &event, &error);
#else
    (void)gpu;
    return false;
#endif
}

static bool mapped_buffer(VkPhysicalDevice physical, VkDevice device, VkDeviceSize bytes,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags flags,
                          dc_present_buffer_t *buffer) {
    VkBufferCreateInfo info = {.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size=bytes,
        .usage=usage, .sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    if (vkCreateBuffer(device, &info, NULL, &buffer->buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i=0; i<props.memoryTypeCount; ++i) {
        if (!(req.memoryTypeBits & (1u<<i)) || (props.memoryTypes[i].propertyFlags & flags)!=flags) continue;
        VkMemoryAllocateInfo allocation = {.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize=req.size, .memoryTypeIndex=i};
        if (vkAllocateMemory(device, &allocation, NULL, &buffer->memory) != VK_SUCCESS) continue;
        return vkBindBufferMemory(device, buffer->buffer, buffer->memory, 0)==VK_SUCCESS &&
            vkMapMemory(device, buffer->memory, 0, bytes, 0, &buffer->mapped)==VK_SUCCESS;
    }
    return false;
}

static void release_buffer(VkDevice device, dc_present_buffer_t buffer) {
    if (buffer.mapped) vkUnmapMemory(device, buffer.memory);
    if (buffer.buffer) vkDestroyBuffer(device, buffer.buffer, NULL);
    if (buffer.memory) vkFreeMemory(device, buffer.memory, NULL);
}

static void release(dc_gpu_t *gpu, dc_software_present_t *s) {
    if (s->device) vkDeviceWaitIdle(s->device);
    release_buffer(s->device, s->upload);
    release_buffer(gpu->device, s->readback);
    if (s->composed) vkDestroyImage(gpu->device, s->composed, NULL);
    if (s->composed_memory) vkFreeMemory(gpu->device, s->composed_memory, NULL);
    if (s->ready) {
        for (uint32_t i=0; i<s->count; ++i)
            if (s->ready[i]) vkDestroySemaphore(s->device, s->ready[i], NULL);
        free(s->ready);
    }
    free(s->images);
    if (s->acquire) vkDestroySemaphore(s->device, s->acquire, NULL);
    if (s->upload_fence) vkDestroyFence(s->device, s->upload_fence, NULL);
    if (s->pool) vkDestroyCommandPool(s->device, s->pool, NULL);
    if (s->chain) vkDestroySwapchainKHR(s->device, s->chain, NULL);
    if (s->device) vkDestroyDevice(s->device, NULL);
    free(s);
}

void dc_software_present_destroy(dc_gpu_t *gpu) {
    if (!gpu->software_present) return;
    release(gpu, gpu->software_present);
    gpu->software_present = NULL;
    gpu->swapchain = VK_NULL_HANDLE;
    gpu->swap_images = NULL;
    gpu->swap_count = 0;
}

static bool supports_format(VkPhysicalDevice p,const VkSwapchainCreateInfoKHR *info) {
    uint32_t count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(p,info->surface,&count,NULL)!=VK_SUCCESS || !count) return false;
    VkSurfaceFormatKHR *formats = calloc(count,sizeof(*formats));
    bool found = false;
    if (formats && vkGetPhysicalDeviceSurfaceFormatsKHR(p,info->surface,&count,formats)==VK_SUCCESS)
        for (uint32_t i=0; i<count; ++i)
            if (formats[i].format==info->imageFormat && formats[i].colorSpace==info->imageColorSpace) found=true;
    free(formats);
    return found;
}

static bool supports_mode(VkPhysicalDevice p,const VkSwapchainCreateInfoKHR *info) {
    if (info->presentMode == VK_PRESENT_MODE_FIFO_KHR) return true;
    uint32_t count = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(p,info->surface,&count,NULL)!=VK_SUCCESS || !count) return false;
    VkPresentModeKHR *modes = calloc(count,sizeof(*modes));
    bool found = false;
    if (modes && vkGetPhysicalDeviceSurfacePresentModesKHR(p,info->surface,&count,modes)==VK_SUCCESS)
        for (uint32_t i=0; i<count; ++i) if (modes[i]==info->presentMode) found=true;
    free(modes);
    return found;
}

static uint32_t present_family(VkPhysicalDevice p,VkSurfaceKHR surface) {
    uint32_t count=0, family=UINT32_MAX;
    vkGetPhysicalDeviceQueueFamilyProperties(p,&count,NULL);
    VkQueueFamilyProperties *families=calloc(count,sizeof(*families));
    if (!families) return family;
    vkGetPhysicalDeviceQueueFamilyProperties(p,&count,families);
    for (uint32_t i=0;i<count;++i) {
        VkBool32 present=VK_FALSE;
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            vkGetPhysicalDeviceSurfaceSupportKHR(p,i,surface,&present)==VK_SUCCESS && present) {
            family=i;
            break;
        }
    }
    free(families);
    return family;
}

static bool prepare(dc_gpu_t *gpu, dc_software_present_t *s, VkPhysicalDevice p,
                    VkSwapchainCreateInfoKHR info) {
    VkSurfaceCapabilitiesKHR caps;
    if (!supports_format(p,&info) || !supports_mode(p,&info) ||
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p,info.surface,&caps)!=VK_SUCCESS ||
        (caps.supportedUsageFlags & info.imageUsage)!=info.imageUsage ||
        !(caps.supportedTransforms & info.preTransform) ||
        !(caps.supportedCompositeAlpha & info.compositeAlpha)) return false;
    VkExtent2D extent=info.imageExtent;
    if (extent.width<caps.minImageExtent.width || extent.width>caps.maxImageExtent.width ||
        extent.height<caps.minImageExtent.height || extent.height>caps.maxImageExtent.height ||
        (caps.currentExtent.width!=UINT32_MAX &&
        (extent.width!=caps.currentExtent.width || extent.height!=caps.currentExtent.height))) return false;
    info.minImageCount=caps.minImageCount+1;
    if (caps.maxImageCount && info.minImageCount>caps.maxImageCount) info.minImageCount=caps.maxImageCount;
    uint32_t family=present_family(p,info.surface);
    if (family==UINT32_MAX) return false;
    float priority=1.f;
    VkDeviceQueueCreateInfo queue={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
    const char *extension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo device={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,
        .pQueueCreateInfos=&queue,.enabledExtensionCount=1,.ppEnabledExtensionNames=&extension};
    if (vkCreateDevice(p,&device,NULL,&s->device)!=VK_SUCCESS) return false;
    vkGetDeviceQueue(s->device,family,0,&s->queue);
    s->bytes=(VkDeviceSize)extent.width*extent.height*4u;
    const VkMemoryPropertyFlags mapped=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!dc_window_transfer_image(gpu,extent,info.imageFormat,&s->composed,&s->composed_memory,NULL,0) ||
        !mapped_buffer(gpu->physical,gpu->device,s->bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       mapped|VK_MEMORY_PROPERTY_HOST_CACHED_BIT,&s->readback) ||
        !mapped_buffer(p,s->device,s->bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       mapped,&s->upload)) return false;
    VkCommandPoolCreateInfo pool={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,.queueFamilyIndex=family};
    if (vkCreateCommandPool(s->device,&pool,NULL,&s->pool)!=VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo command={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool=s->pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkFenceCreateInfo fence={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkSemaphoreCreateInfo semaphore={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkAllocateCommandBuffers(s->device,&command,&s->command)!=VK_SUCCESS ||
        vkCreateFence(s->device,&fence,NULL,&s->upload_fence)!=VK_SUCCESS ||
        vkCreateSemaphore(s->device,&semaphore,NULL,&s->acquire)!=VK_SUCCESS ||
        vkCreateSwapchainKHR(s->device,&info,NULL,&s->chain)!=VK_SUCCESS ||
        vkGetSwapchainImagesKHR(s->device,s->chain,&s->count,NULL)!=VK_SUCCESS || !s->count) return false;
    s->images=calloc(s->count,sizeof(*s->images));
    s->ready=calloc(s->count,sizeof(*s->ready));
    if (!s->images || !s->ready || vkGetSwapchainImagesKHR(s->device,s->chain,&s->count,s->images)!=VK_SUCCESS)
        return false;
    for (uint32_t i=0;i<s->count;++i)
        if (vkCreateSemaphore(s->device,&semaphore,NULL,&s->ready[i])!=VK_SUCCESS) return false;
    return true;
}

bool dc_software_present_create(dc_gpu_t *gpu, const VkSwapchainCreateInfoKHR *info) {
    if (!use_local_presenter(gpu)) return false;
    if (info->imageFormat != VK_FORMAT_R8G8B8A8_UNORM && info->imageFormat != VK_FORMAT_B8G8R8A8_UNORM &&
        info->imageFormat != VK_FORMAT_R8G8B8A8_SRGB && info->imageFormat != VK_FORMAT_B8G8R8A8_SRGB) return false;
    uint32_t count=0;
    if (vkEnumeratePhysicalDevices(gpu->instance,&count,NULL)!=VK_SUCCESS || !count) return false;
    VkPhysicalDevice *devices=calloc(count,sizeof(*devices));
    if (devices && vkEnumeratePhysicalDevices(gpu->instance,&count,devices)==VK_SUCCESS) {
        for (uint32_t i=0;i<count;++i) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i],&props);
            if (props.deviceType!=VK_PHYSICAL_DEVICE_TYPE_CPU || devices[i]==gpu->physical) continue;
            dc_software_present_t *s=calloc(1,sizeof(*s));
            if (!s) break;
            if (prepare(gpu,s,devices[i],*info)) {
                gpu->software_present=s;
                gpu->swapchain=s->chain;
                gpu->swap_images=s->images;
                gpu->swap_count=s->count;
                break;
            }
            release(gpu,s);
        }
    }
    free(devices);
    return gpu->software_present!=NULL;
}
