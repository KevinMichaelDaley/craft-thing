#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test_window.h"

static int g_pass, g_fail;
static VkDevice software_device, swap_device;
static unsigned created, destroyed, rejected;
static bool reject_swap;

#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

VkResult __real_vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *);
VkResult __wrap_vkCreateDevice(VkPhysicalDevice p, const VkDeviceCreateInfo *info, const VkAllocationCallbacks *a, VkDevice *out) {
    VkResult r = __real_vkCreateDevice(p, info, a, out);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(p, &props);
    if (r == VK_SUCCESS && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        software_device = *out;
        ++created;
    }
    return r;
}

void __real_vkDestroyDevice(VkDevice, const VkAllocationCallbacks *);
void __wrap_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *a) {
    if (device == software_device) {
        ++destroyed;
        software_device = VK_NULL_HANDLE;
    }
    __real_vkDestroyDevice(device, a);
}

VkResult __real_vkCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
VkResult __wrap_vkCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *a, VkSwapchainKHR *out) {
    if (reject_swap && d == software_device) {
        ++rejected;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = __real_vkCreateSwapchainKHR(d, info, a, out);
    if (result == VK_SUCCESS) swap_device = d;
    return result;
}

static dc_gpu_t *window(void) {
    dc_gpu_t *g = NULL;
    char err[256] = {0};
    if (!dc_test_create_window(&g,64,64,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)))
        fprintf(stderr,"%s\n",err);
    return g;
}

static bool offers_cpu(dc_gpu_t *g) {
    uint32_t n = 0;
    if (strcmp(SDL_GetCurrentVideoDriver(),"x11") || vkEnumeratePhysicalDevices(g->instance,&n,NULL) != VK_SUCCESS) return false;
    VkPhysicalDevice *devices = calloc(n,sizeof(*devices));
    bool found = false;
    if (devices && vkEnumeratePhysicalDevices(g->instance,&n,devices) == VK_SUCCESS) {
        for (uint32_t i=0; i<n; ++i) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i],&props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) found = true;
        }
    }
    free(devices);
    return found;
}

static bool pixels_match(dc_gpu_t *g, uint32_t seed) {
    uint32_t pixels[64*64];
    uint32_t *source = g->mapped;
    for (unsigned i=0; i<64*64; ++i) source[i] = 0xff000000u | ((seed + i*5393u)&0xffffffu);
    char err[256] = {0};
    if (!dc_gpu_capture_present(g,pixels,64*64,err,sizeof(err))) {
        fprintf(stderr,"%s\n",err);
        return false;
    }
    return memcmp(source,pixels,sizeof(pixels)) == 0;
}

static void test_two_windows_keep_separate_presenters_and_exact_pixels(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT","1",1);
    dc_gpu_t *first = window();
    ASSERT_TRUE(first);
    bool expected = offers_cpu(first);
    VkDevice first_swap = swap_device;
    dc_gpu_t *second = window();
    ASSERT_TRUE(second);
    bool separate = first_swap != first->device && swap_device != second->device && first_swap != swap_device;
    bool correct = pixels_match(first,17) && pixels_match(second,1234);
    dc_gpu_destroy(second);
    correct = correct && pixels_match(first,54321);
    dc_gpu_destroy(first);
    ASSERT_TRUE(correct);
    if (expected) ASSERT_TRUE(separate);
    PASS();
}

static void test_failed_software_swapchain_releases_device_and_uses_native(void) {
    unsigned before = destroyed;
    reject_swap = true;
    dc_gpu_t *gpu = window();
    reject_swap = false;
    ASSERT_TRUE(gpu);
    bool expected = offers_cpu(gpu);
    bool native = swap_device == gpu->device && pixels_match(gpu,39);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(native);
    if (expected) { ASSERT_TRUE(rejected); ASSERT_TRUE(destroyed > before); }
    PASS();
}

static void test_disabled_software_presentation_uses_render_device(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT","0",1);
    unsigned before = created;
    dc_gpu_t *gpu = window();
    ASSERT_TRUE(gpu);
    bool native = swap_device == gpu->device && pixels_match(gpu,731);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(native);
    ASSERT_INT_EQ(created,before);
    PASS();
}

int main(void) {
    RUN(test_two_windows_keep_separate_presenters_and_exact_pixels);
    RUN(test_failed_software_swapchain_releases_device_and_uses_native);
    RUN(test_disabled_software_presentation_uses_render_device);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail != 0;
}
