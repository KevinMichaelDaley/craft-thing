#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL_vulkan.h>

#include "body_features_gpu.h"
#include "gpu_internal.h"
#include "present_mode_latest_ready.h"
#include "present_mode_select.h"

struct dc_gpu_device_lifetime {
    VkDebugUtilsMessengerEXT validation_messenger;
    atomic_uint validation_errors;
    uint32_t references;
};

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

static bool gpu_supports_latest_ready(VkPhysicalDevice physical) {
    uint32_t count=0;
    if(vkEnumerateDeviceExtensionProperties(physical,NULL,&count,NULL)!=VK_SUCCESS||!count) return false;
    VkExtensionProperties *extensions=calloc(count,sizeof(*extensions));
    if(!extensions) return false;
    VkResult result=vkEnumerateDeviceExtensionProperties(physical,NULL,&count,extensions);
    bool supported=false;
    if(result==VK_SUCCESS) for(uint32_t i=0;i<count;i++) {
        if(strcmp(extensions[i].extensionName,DC_VK_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION)==0) {
            supported=true;
            break;
        }
    }
    free(extensions);
    return supported;
}
void dc_gpu_startup_trace(const char *stage, Uint64 started) {
    if(getenv("DC_BONFIRE_STARTUP_TRACE"))
        fprintf(stderr,"GPU startup %s: %llums\n",stage,
                (unsigned long long)(SDL_GetTicks64()-started));
}
enum { DC_PIPELINE_CACHE_MAX_BYTES = 128u * 1024u * 1024u };
static bool pipeline_cache_directory_ready(const char *path) {
    if(mkdir(path,0700)==0) return true;
    struct stat info;
    return errno==EEXIST&&stat(path,&info)==0&&S_ISDIR(info.st_mode);
}
static void pipeline_cache_load(dc_gpu_t *gpu) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical,&properties);
    char uuid[VK_UUID_SIZE*2u+1u];
    for(uint32_t i=0;i<VK_UUID_SIZE;i++)
        snprintf(uuid+i*2u,3u,"%02x",properties.pipelineCacheUUID[i]);
    char name[96],legacy[sizeof(gpu->pipeline_cache_path)];
    snprintf(name,sizeof(name),"pipeline_cache_%04x_%04x_%08x_%s.bin",
             properties.vendorID,properties.deviceID,properties.driverVersion,uuid);
    snprintf(legacy,sizeof(legacy),"build/%s",name);
    const char *directory=getenv("DC_PIPELINE_CACHE_DIR");
    char root[sizeof(gpu->pipeline_cache_path)],user_directory[sizeof(gpu->pipeline_cache_path)];
    if(!directory||!directory[0]) {
        directory=getenv("XDG_CACHE_HOME");
        if(!directory||directory[0]!='/') {
            const char *home=getenv("HOME");
            directory=home&&snprintf(root,sizeof(root),"%s/.cache",home)<(int)sizeof(root)?root:NULL;
        }
        if(directory&&directory[0]=='/'&&
           pipeline_cache_directory_ready(directory)&&
           snprintf(user_directory,sizeof(user_directory),"%s/dungeoncraft",directory)<(int)sizeof(user_directory))
            directory=user_directory;
        else directory=NULL;
    }
    if(!directory||!pipeline_cache_directory_ready(directory)||
       snprintf(gpu->pipeline_cache_path,sizeof(gpu->pipeline_cache_path),"%s/%s",directory,name)>=
           (int)sizeof(gpu->pipeline_cache_path))
        snprintf(gpu->pipeline_cache_path,sizeof(gpu->pipeline_cache_path),"%s",legacy);
    void *bytes=NULL;size_t length=0u;
    FILE *file=fopen(gpu->pipeline_cache_path,"rb");
    if(!file&&strcmp(gpu->pipeline_cache_path,legacy)!=0) file=fopen(legacy,"rb");
    if(file) {
        if(fseek(file,0,SEEK_END)==0) {
            long size=ftell(file);
            if(size>0&&(unsigned long)size<=DC_PIPELINE_CACHE_MAX_BYTES&&
               fseek(file,0,SEEK_SET)==0) {
                bytes=malloc((size_t)size);
                if(bytes&&fread(bytes,1u,(size_t)size,file)==(size_t)size)
                    length=(size_t)size;
            }
        }
        fclose(file);
    }
    VkPipelineCacheCreateInfo info={.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize=length,.pInitialData=length?bytes:NULL};
    VkResult result=vkCreatePipelineCache(gpu->device,&info,NULL,&gpu->pipeline_cache);
    if(result!=VK_SUCCESS&&length) {
        info.initialDataSize=0u;info.pInitialData=NULL;
        result=vkCreatePipelineCache(gpu->device,&info,NULL,&gpu->pipeline_cache);
        length=0u;
    }
    gpu->pipeline_cache_loaded=result==VK_SUCCESS&&length>0u;
    if(getenv("DC_PIPELINE_CACHE_TRACE"))
        fprintf(stderr,"Pipeline cache load result%d bytes%zu loaded%d\n",
                (int)result,length,gpu->pipeline_cache_loaded);
    if(result!=VK_SUCCESS) gpu->pipeline_cache_path[0]=0;
    free(bytes);
}
static void pipeline_cache_save(dc_gpu_t *gpu) {
    if(!gpu->pipeline_cache) return;
    size_t length=0u;
    VkResult query_result=gpu->pipeline_cache_path[0]?
        vkGetPipelineCacheData(gpu->device,gpu->pipeline_cache,&length,NULL):VK_ERROR_INITIALIZATION_FAILED;
    if(getenv("DC_PIPELINE_CACHE_TRACE"))
        fprintf(stderr,"Pipeline cache save query result%d bytes%zu limit%u\n",
                (int)query_result,length,(unsigned)DC_PIPELINE_CACHE_MAX_BYTES);
    if(query_result==VK_SUCCESS&&length>0u&&length<=DC_PIPELINE_CACHE_MAX_BYTES) {
        void *bytes=malloc(length);
        if(bytes&&vkGetPipelineCacheData(gpu->device,gpu->pipeline_cache,
                                          &length,bytes)==VK_SUCCESS) {
            char temporary[sizeof(gpu->pipeline_cache_path)+32u];
            snprintf(temporary,sizeof(temporary),"%s.tmp.%ld",
                     gpu->pipeline_cache_path,(long)getpid());
            FILE *file=fopen(temporary,"wb");
            if(file) {
                bool written=fwrite(bytes,1u,length,file)==length;
                if(fclose(file)!=0) written=false;
                if(!written||rename(temporary,gpu->pipeline_cache_path)!=0)
                    remove(temporary);
            }
        }
        free(bytes);
    }
    vkDestroyPipelineCache(gpu->device,gpu->pipeline_cache,NULL);
    gpu->pipeline_cache=VK_NULL_HANDLE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL dc_gpu_validation_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)types;
    (void)data;
    struct dc_gpu_device_lifetime *lifetime = user;
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        atomic_fetch_add_explicit(&lifetime->validation_errors, 1u, memory_order_relaxed);
    return VK_FALSE;
}

uint32_t dc_gpu_validation_errors(const dc_gpu_t *gpu) {
    return gpu && gpu->device_lifetime ?
        atomic_load_explicit(&gpu->device_lifetime->validation_errors, memory_order_relaxed) : 0u;
}

static void dc_gpu_create_validation_messenger(dc_gpu_t *gpu) {
    PFN_vkCreateDebugUtilsMessengerEXT create =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            gpu->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!create) return;
    VkDebugUtilsMessengerCreateInfoEXT messenger = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = dc_gpu_validation_callback,
        .pUserData = gpu->device_lifetime};
    create(gpu->instance, &messenger, NULL, &gpu->device_lifetime->validation_messenger);
}

void dc_gpu_release_device(dc_gpu_t *gpu) {
    struct dc_gpu_device_lifetime *lifetime = gpu->device_lifetime;
    if (!lifetime || --lifetime->references != 0u) return;
    if (gpu->device) {
        pipeline_cache_save(gpu);
        vkDestroyDevice(gpu->device, NULL);
    }
    if (lifetime->validation_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                gpu->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy) destroy(gpu->instance, lifetime->validation_messenger, NULL);
    }
    if (gpu->instance) vkDestroyInstance(gpu->instance, NULL);
    free(lifetime);
}

bool dc_gpu_initialize_device(dc_gpu_t *gpu, VkDeviceSize bytes, uint32_t window_width,
                              uint32_t window_height, Uint64 started, char *err, uint32_t cap) {
    gpu->device_lifetime = calloc(1, sizeof(*gpu->device_lifetime));
    if (!gpu->device_lifetime) return error(err, cap, "Out of memory creating Vulkan device ownership");
    gpu->device_lifetime->references = 1u;
    atomic_init(&gpu->device_lifetime->validation_errors, 0u);
    if (window_width && window_height) {
        const char *wayland=getenv("WAYLAND_DISPLAY");
        if(wayland&&*wayland&&!SDL_GetHint(SDL_HINT_VIDEODRIVER)&&
           !(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO))
            SDL_SetHint(SDL_HINT_VIDEODRIVER,"wayland");
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            return error(err, cap, SDL_GetError());
        }
        uint32_t flags = SDL_WINDOW_VULKAN;
        const char *fullscreen=getenv("DC_FULLSCREEN");
        if(fullscreen&&strcmp(fullscreen,"0")!=0) flags|=SDL_WINDOW_FULLSCREEN_DESKTOP;
#ifdef DC_NATIVE_VIEW
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
        gpu->window = SDL_CreateWindow("Dungeoncraft GPU testbed", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, (int)window_width, (int)window_height, flags);
        if (!gpu->window) {
            error(err, cap, SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }
        dc_gpu_startup_trace("SDL window",started);
    }
    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "dungeoncraft", .apiVersion = VK_API_VERSION_1_3 };
    VkInstanceCreateInfo instance_info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app };
    unsigned int extension_count = 0;
    const char **extensions = NULL;
    if (gpu->window) {
        if (!SDL_Vulkan_GetInstanceExtensions(gpu->window, &extension_count, NULL)) {
            return error(err, cap, SDL_GetError());
        }
        extensions = calloc(extension_count, sizeof(*extensions));
        if (!extensions || !SDL_Vulkan_GetInstanceExtensions(gpu->window, &extension_count, extensions)) {
            free(extensions);
            return error(err, cap, "Cannot query SDL Vulkan extensions");
        }
        instance_info.enabledExtensionCount = extension_count;
        instance_info.ppEnabledExtensionNames = extensions;
    }
    const char *validation = getenv("DC_VK_VALIDATE");
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *debug_utils_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    if (validation && validation[0] == '1' && !gpu->window) {
        instance_info.enabledExtensionCount = 1;
        instance_info.ppEnabledExtensionNames = &debug_utils_extension;
    }
    if (validation && validation[0] == '1') {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties *layers = calloc(layer_count, sizeof(*layers));
        if (!layers) {
            free(extensions);
            return error(err, cap, "Cannot list Vulkan validation layers");
        }
        vkEnumerateInstanceLayerProperties(&layer_count, layers);
        bool present = false;
        for (uint32_t i = 0; i < layer_count; ++i)
            if (strcmp(layers[i].layerName, layer) == 0) present = true;
        free(layers);
        if (!present) {
            free(extensions);
            return error(err, cap, "VK_LAYER_KHRONOS_validation is not installed");
        }
        instance_info.enabledLayerCount = 1;
        instance_info.ppEnabledLayerNames = &layer;
    }
    VkResult instance_result = vkCreateInstance(&instance_info, NULL, &gpu->instance);
    free(extensions);
    if (instance_result != VK_SUCCESS) {
        return error(err, cap, "Cannot create Vulkan 1.3 instance");
    }
    if (validation && validation[0] == '1') dc_gpu_create_validation_messenger(gpu);
    dc_gpu_startup_trace("Vulkan instance",started);
    if (gpu->window && !SDL_Vulkan_CreateSurface(gpu->window, gpu->instance, &gpu->surface)) {
        return error(err, cap, SDL_GetError());
    }
    if (!dc_gpu_pick_device(gpu, bytes, err, cap)) return false;
    printf("Vulkan device: %s (type %d vendor %04x)\n", gpu->device_name,
           gpu->device_type, gpu->vendor_id);
    dc_gpu_body_features_t body_features;
    if (!dc_gpu_body_features_query(gpu->physical, &body_features, err, cap)) return false;
    const char *latency_setting=getenv("DC_LOW_LATENCY_PRESENT");
    bool latest_ready_available=gpu->window&&gpu_supports_latest_ready(gpu->physical);
    gpu->low_latency_present=dc_present_low_latency_requested(latency_setting,latest_ready_available);
    dc_vk_present_mode_fifo_latest_ready_features_t latest_ready={
        .sType=DC_VK_PRESENT_MODE_FIFO_LATEST_READY_FEATURES};
    if(gpu->window&&gpu->low_latency_present&&latest_ready_available) {
        VkPhysicalDeviceFeatures2 available={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext=&latest_ready};
        vkGetPhysicalDeviceFeatures2(gpu->physical,&available);
        if(latest_ready.presentModeFifoLatestReady) {
            gpu->latest_ready_enabled=true;
            body_features.subgroups.pNext=&latest_ready;
        }
    }
    if(!latency_setting&&!gpu->latest_ready_enabled) gpu->low_latency_present=false;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = gpu->family, .queueCount = 1, .pQueuePriorities = &priority };
    VkDeviceCreateInfo device_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &body_features.synchronization, .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info };
    const char *swap_extensions[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        DC_VK_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION};
    if (gpu->window) {
        device_info.enabledExtensionCount = gpu->latest_ready_enabled?2u:1u;
        device_info.ppEnabledExtensionNames = swap_extensions;
    }
    if (vkCreateDevice(gpu->physical, &device_info, NULL, &gpu->device) != VK_SUCCESS) {
        return error(err, cap, "Cannot create Vulkan compute device");
    }
    vkGetDeviceQueue(gpu->device, gpu->family, 0, &gpu->queue);
    pipeline_cache_load(gpu);
    dc_gpu_startup_trace("Vulkan device",started);
    return true;
}

bool dc_gpu_inherit_device(dc_gpu_t *gpu, const dc_gpu_t *parent, VkDeviceSize bytes,
                           char *err, uint32_t cap) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(parent->physical, &properties);
    if (bytes > properties.limits.maxStorageBufferRange)
        return error(err, cap, "Shared Vulkan device does not support this grid");
    struct dc_gpu_device_lifetime *lifetime = parent->device_lifetime;
    if (lifetime->references == UINT32_MAX)
        return error(err, cap, "Too many shared Vulkan contexts");

    ++lifetime->references;
    gpu->device_lifetime = lifetime;
    gpu->instance = parent->instance;
    gpu->physical = parent->physical;
    gpu->device = parent->device;
    gpu->queue = parent->queue;
    gpu->family = parent->family;
    gpu->vendor_id = parent->vendor_id;
    gpu->device_type = parent->device_type;
    memcpy(gpu->device_name, parent->device_name, sizeof(gpu->device_name));
    gpu->pipeline_cache = parent->pipeline_cache;
    gpu->pipeline_cache_loaded = parent->pipeline_cache_loaded;
    memcpy(gpu->pipeline_cache_path, parent->pipeline_cache_path, sizeof(gpu->pipeline_cache_path));
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
