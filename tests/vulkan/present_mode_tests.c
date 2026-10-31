#include <stdio.h>

#include "../../src/vulkan/present_mode_select.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_default_prefers_tear_free_fifo(void) {
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR};
    ASSERT_EQ(dc_present_mode_select(modes,2,false,false),VK_PRESENT_MODE_FIFO_KHR);
    PASS();
}

static void test_low_latency_uses_supported_immediate_mode(void) {
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR};
    ASSERT_EQ(dc_present_mode_select(modes,2,true,false),VK_PRESENT_MODE_IMMEDIATE_KHR);
    PASS();
}

static void test_mailbox_wins_without_tearing(void) {
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR,VK_PRESENT_MODE_MAILBOX_KHR};
    ASSERT_EQ(dc_present_mode_select(modes,3,true,false),VK_PRESENT_MODE_MAILBOX_KHR);
    PASS();
}

static void test_low_latency_prefers_tear_free_latest_ready(void) {
    const VkPresentModeKHR latest_ready=DC_VK_PRESENT_MODE_FIFO_LATEST_READY;
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR,latest_ready};
    ASSERT_EQ(dc_present_mode_select(modes,3,true,true),latest_ready);
    PASS();
}

static void test_latest_ready_requires_enabled_feature(void) {
    const VkPresentModeKHR modes[]={VK_PRESENT_MODE_FIFO_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR,
                                    DC_VK_PRESENT_MODE_FIFO_LATEST_READY};
    ASSERT_EQ(dc_present_mode_select(modes,3,true,false),VK_PRESENT_MODE_IMMEDIATE_KHR);
    ASSERT_EQ(dc_present_mode_select(modes,3,false,true),VK_PRESENT_MODE_FIFO_KHR);
    PASS();
}

int main(void) {
    RUN(test_default_prefers_tear_free_fifo);
    RUN(test_low_latency_uses_supported_immediate_mode);
    RUN(test_mailbox_wins_without_tearing);
    RUN(test_low_latency_prefers_tear_free_latest_ready);
    RUN(test_latest_ready_requires_enabled_feature);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
