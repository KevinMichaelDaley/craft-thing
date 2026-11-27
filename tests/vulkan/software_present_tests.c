#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test_window.h"
#if defined(DC_HAVE_X11) && defined(SDL_VIDEO_DRIVER_X11)
#include <SDL_syswm.h>
#include <X11/Xutil.h>
#include <sys/socket.h>
#endif

static int g_pass, g_fail;
static VkDevice software_device, swap_device;
static unsigned created, destroyed, rejected;
static bool reject_swap, reject_images;
static bool have_render_device;

#define RUN(fn)                                                                                        \
    do {                                                                                               \
        printf("RUN %s\n", #fn);                                                                       \
        fn();                                                                                          \
    } while (0)
#define ASSERT_TRUE(x)                                                                                 \
    do {                                                                                               \
        if (!(x)) {                                                                                    \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                     \
            ++g_fail;                                                                                  \
            return;                                                                                    \
        }                                                                                              \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

VkResult __real_vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *,
                               const VkAllocationCallbacks *, VkDevice *);
VkResult __wrap_vkCreateDevice(VkPhysicalDevice p, const VkDeviceCreateInfo *info,
                               const VkAllocationCallbacks *a, VkDevice *out) {
    VkResult r = __real_vkCreateDevice(p, info, a, out);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(p, &props);
    if (r == VK_SUCCESS && have_render_device && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        software_device = *out;
        ++created;
    }
    if (r == VK_SUCCESS) have_render_device = true;
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

VkResult __real_vkCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR *,
                                     const VkAllocationCallbacks *, VkSwapchainKHR *);
VkResult __wrap_vkCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *info,
                                     const VkAllocationCallbacks *a, VkSwapchainKHR *out) {
    if (reject_swap && d == software_device) {
        ++rejected;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = __real_vkCreateSwapchainKHR(d, info, a, out);
    if (result == VK_SUCCESS) swap_device = d;
    return result;
}

VkResult __real_vkGetSwapchainImagesKHR(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
VkResult __wrap_vkGetSwapchainImagesKHR(VkDevice d, VkSwapchainKHR chain, uint32_t *count,
                                        VkImage *images) {
    if (reject_images && d == software_device) {
        ++rejected;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    return __real_vkGetSwapchainImagesKHR(d, chain, count, images);
}

static dc_gpu_t *window(void) {
    have_render_device = false;
    dc_gpu_t *g = NULL;
    char err[256] = {0};
    if (!dc_test_create_window(&g, 64, 64, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)))
        fprintf(stderr, "%s\n", err);
    return g;
}

static bool offers_cpu(dc_gpu_t *g) {
#if !defined(DC_HAVE_X11) || !defined(SDL_VIDEO_DRIVER_X11)
    (void)g;
    return false;
#else
    uint32_t n = 0;
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    struct sockaddr_storage peer;
    socklen_t size = sizeof(peer);
    if (!SDL_GetWindowWMInfo(g->window, &wm) || wm.subsystem != SDL_SYSWM_X11 ||
        getpeername(ConnectionNumber(wm.info.x11.display), (struct sockaddr *)&peer, &size) ||
        peer.ss_family != AF_UNIX ||
        vkEnumeratePhysicalDevices(g->instance, &n, NULL) != VK_SUCCESS)
        return false;
    VkPhysicalDevice *devices = calloc(n, sizeof(*devices));
    bool found = false;
    if (devices && vkEnumeratePhysicalDevices(g->instance, &n, devices) == VK_SUCCESS) {
        for (uint32_t i = 0; i < n; ++i) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i], &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU && devices[i] != g->physical) {
                uint32_t queues = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queues, NULL);
                for (uint32_t q = 0; q < queues; ++q) {
                    VkBool32 supported = VK_FALSE;
                    if (vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], q, g->surface, &supported) ==
                            VK_SUCCESS && supported)
                        found = true;
                }
            }
        }
    }
    free(devices);
    return found;
#endif
}

static bool pixels_match(dc_gpu_t *g, uint32_t seed) {
    uint32_t pixels[64 * 64];
    uint32_t *source = g->mapped;
    for (unsigned i = 0; i < 64 * 64; ++i)
        source[i] = 0xff000000u | ((seed + i * 5393u) & 0xffffffu);
    char err[256] = {0};
    if (!dc_gpu_capture_present(g, pixels, 64 * 64, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        return false;
    }
    return memcmp(source, pixels, sizeof(pixels)) == 0;
}

static void test_two_windows_keep_separate_presenters_and_exact_pixels(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT", "1", 1);
    dc_gpu_t *first = window();
    ASSERT_TRUE(first);
    bool expected = offers_cpu(first);
    VkDevice first_swap = swap_device;
    dc_gpu_t *second = window();
    ASSERT_TRUE(second);
    bool separate =
        first_swap != first->device && swap_device != second->device && first_swap != swap_device;
    bool correct = pixels_match(first, 17) && pixels_match(second, 1234);
    dc_gpu_destroy(second);
    correct = correct && pixels_match(first, 54321);
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
    bool native = swap_device == gpu->device && pixels_match(gpu, 39);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(native);
    if (expected) {
        ASSERT_TRUE(rejected);
        ASSERT_TRUE(destroyed > before);
    }
    PASS();
}

static void test_disabled_software_presentation_uses_render_device(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT", "0", 1);
    unsigned before = created;
    dc_gpu_t *gpu = window();
    ASSERT_TRUE(gpu);
    bool native = swap_device == gpu->device && pixels_match(gpu, 731);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(native);
    ASSERT_INT_EQ(created, before);
    PASS();
}

static void test_partially_created_swapchain_falls_back(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT", "1", 1);
    unsigned before = rejected;
    reject_images = true;
    dc_gpu_t *gpu = window();
    reject_images = false;
    ASSERT_TRUE(gpu);
    bool expected = offers_cpu(gpu);
    bool valid =
        swap_device == gpu->device && pixels_match(gpu, 83) && dc_gpu_validation_errors(gpu) == 0;
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(valid);
    if (expected) ASSERT_TRUE(rejected > before);
    PASS();
}

#if defined(DC_HAVE_X11) && defined(SDL_VIDEO_DRIVER_X11)
static bool visible_pattern(dc_gpu_t *gpu) {
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(gpu->window, &wm) || wm.subsystem != SDL_SYSWM_X11) return false;
    Display *display = wm.info.x11.display;
    Window window_id = wm.info.x11.window;
    SDL_ShowWindow(gpu->window);
    for (unsigned attempt = 0; attempt < 60; ++attempt) {
        SDL_PumpEvents();
        XWindowAttributes attributes;
        if (XGetWindowAttributes(display, window_id, &attributes) &&
            attributes.map_state == IsViewable) {
            if (!pixels_match(gpu, 197u)) return false;
            XSync(display, False);
            XImage *image = XGetImage(display, window_id, 0, 0, 64, 64, AllPlanes, ZPixmap);
            if (!image) return false;
            bool matched = true;
            for (unsigned y = 0; y < 64; ++y)
                for (unsigned x = 0; x < 64; ++x) {
                    uint32_t value = ((uint32_t *)gpu->mapped)[64 * y + x];
                    uint32_t rgb = ((value & 255) << 16) | (value & 0xff00) | ((value >> 16) & 255);
                    if ((XGetPixel(image, x, y) & 0xffffff) != rgb) matched = false;
                }
            XDestroyImage(image);
            if (matched) return true;
        }
        SDL_Delay(10);
    }
    return false;
}

static void test_visible_x11_window_matches_composed_pixels(void) {
    SDL_setenv("DC_X11_SOFTWARE_PRESENT", "1", 1);
    dc_gpu_t *gpu = window();
    ASSERT_TRUE(gpu);
    bool correct = swap_device == gpu->device || visible_pattern(gpu);
    dc_gpu_destroy(gpu);
    ASSERT_TRUE(correct);
    PASS();
}
#endif

int main(void) {
    RUN(test_two_windows_keep_separate_presenters_and_exact_pixels);
    RUN(test_failed_software_swapchain_releases_device_and_uses_native);
    RUN(test_partially_created_swapchain_falls_back);
#if defined(DC_HAVE_X11) && defined(SDL_VIDEO_DRIVER_X11)
    RUN(test_visible_x11_window_matches_composed_pixels);
#endif
    RUN(test_disabled_software_presentation_uses_render_device);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
