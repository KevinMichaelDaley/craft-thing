#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define PASS() g_pass++

static VkPhysicalDeviceProperties props(VkPhysicalDeviceType type,
                                        uint32_t vendor, uint32_t api) {
    VkPhysicalDeviceProperties p = {0};
    p.deviceType = type;
    p.vendorID = vendor;
    p.apiVersion = api;
    snprintf(p.deviceName, sizeof(p.deviceName), "fake-%u", vendor);
    return p;
}
#define REQUIRED (64u * 1024u * 1024u)
static bool compute_ok = true, transfer_ok = true, present_ok = true;
static int score_of(VkPhysicalDeviceProperties p) {
    return dc_gpu_device_score(&p, 512u * 1024u * 1024u, REQUIRED, compute_ok,
                               transfer_ok, present_ok);
}
static int score_storage(VkDeviceSize max_storage) {
    VkPhysicalDeviceProperties p =
        props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de, VK_API_VERSION_1_3);
    return dc_gpu_device_score(&p, max_storage, REQUIRED, compute_ok,
                               transfer_ok, present_ok);
}
static void test_software_rasterizers_are_never_selected(void) {
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_CPU, 0x10005,
                               VK_API_VERSION_1_3)) < 0);
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_CPU, 0x1af4,
                               VK_API_VERSION_1_3)) < 0);
    PASS();
}
static void test_unqualified_or_insufficient_devices_are_never_selected(void) {
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                               VK_API_VERSION_1_2)) < 0);
    ASSERT_TRUE(score_storage(16u * 1024u * 1024u) < 0);
    ASSERT_TRUE(score_storage(REQUIRED) > 0);
    PASS();
}
static void test_discrete_is_preferred_over_integrated_and_unknown(void) {
    int discrete = score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                                  VK_API_VERSION_1_3));
    int integrated = score_of(props(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
                                    0x8086, VK_API_VERSION_1_3));
    int unknown = score_of(props(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU, 0x1af4,
                                 VK_API_VERSION_1_3));
    ASSERT_TRUE(discrete > 0);
    ASSERT_TRUE(integrated > 0);
    ASSERT_TRUE(discrete > integrated);
    ASSERT_TRUE(integrated > unknown);
    PASS();
}
static void test_capable_devices_keep_their_relative_order(void) {
    VkPhysicalDeviceProperties first =
        props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de, VK_API_VERSION_1_3);
    VkPhysicalDeviceProperties second =
        props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x1002, VK_API_VERSION_1_3);
    ASSERT_EQ(score_of(first), score_of(second));
    PASS();
}
static void test_missing_capabilities_are_rejected_not_scored(void) {
    compute_ok = false;
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                               VK_API_VERSION_1_3)) < 0);
    compute_ok = true;
    transfer_ok = false;
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                               VK_API_VERSION_1_3)) < 0);
    transfer_ok = true;
    present_ok = false;
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                               VK_API_VERSION_1_3)) < 0);
    present_ok = true;
    ASSERT_TRUE(score_of(props(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10de,
                               VK_API_VERSION_1_3)) > 0);
    PASS();
}
int main(void) {
    printf("=== Device Selection Tests ===\n\n");
    RUN(test_software_rasterizers_are_never_selected);
    RUN(test_unqualified_or_insufficient_devices_are_never_selected);
    RUN(test_discrete_is_preferred_over_integrated_and_unknown);
    RUN(test_capable_devices_keep_their_relative_order);
    RUN(test_missing_capabilities_are_rejected_not_scored);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
