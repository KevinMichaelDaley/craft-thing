#ifndef DC_VULKAN_PRESENT_MODE_SELECT_H
#define DC_VULKAN_PRESENT_MODE_SELECT_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "present_mode_latest_ready.h"

static inline VkPresentModeKHR dc_present_mode_select(const VkPresentModeKHR *modes,
                                                       uint32_t count,bool low_latency,
                                                       bool latest_ready_enabled) {
    bool mailbox=false,latest_ready=false,immediate=false;
    for(uint32_t i=0;i<count;i++) {
        if(modes[i]==VK_PRESENT_MODE_MAILBOX_KHR) mailbox=true;
        if(modes[i]==DC_VK_PRESENT_MODE_FIFO_LATEST_READY) latest_ready=true;
        if(modes[i]==VK_PRESENT_MODE_IMMEDIATE_KHR) immediate=true;
    }
    if(mailbox) return VK_PRESENT_MODE_MAILBOX_KHR;
    if(low_latency&&latest_ready_enabled&&latest_ready) return DC_VK_PRESENT_MODE_FIFO_LATEST_READY;
    if(low_latency&&immediate) return VK_PRESENT_MODE_IMMEDIATE_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}

#endif
