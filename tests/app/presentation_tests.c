#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static VkPresentModeKHR mode;
static bool presenting, finite_acquire;
static unsigned idle_calls;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() ++g_pass

VkResult __real_vkCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
VkResult __wrap_vkCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *i, const VkAllocationCallbacks *a, VkSwapchainKHR *s) {
    mode=i->presentMode;
    return __real_vkCreateSwapchainKHR(d,i,a,s);
}
VkResult __real_vkAcquireNextImageKHR(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *);
VkResult __wrap_vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t t,VkSemaphore sem,VkFence f,uint32_t *i) {
    finite_acquire=finite_acquire && t!=UINT64_MAX;
    return __real_vkAcquireNextImageKHR(d,s,t,sem,f,i);
}
VkResult __real_vkQueueWaitIdle(VkQueue);
VkResult __wrap_vkQueueWaitIdle(VkQueue q) {
    if(presenting) ++idle_calls;
    return __real_vkQueueWaitIdle(q);
}

static int window_cycle(void) {
    char err[256]={0}; dc_gpu_t *g=NULL;
    if(!dc_gpu_create_window(&g,64,64,256,256,"build/shaders/pattern.comp.spv",err,sizeof(err))) {
        fprintf(stderr,"%s\n",err); return 1;
    }
    uint32_t count=0;
    if(vkGetPhysicalDeviceSurfacePresentModesKHR(g->physical,g->surface,&count,NULL)!=VK_SUCCESS) return 2;
    VkPresentModeKHR *modes=calloc(count,sizeof(*modes)); if(!modes) return 3;
    if(vkGetPhysicalDeviceSurfacePresentModesKHR(g->physical,g->surface,&count,modes)!=VK_SUCCESS) return 4;
    bool mailbox=false;
    for(uint32_t i=0;i<count;++i) mailbox=mailbox || modes[i]==VK_PRESENT_MODE_MAILBOX_KHR;
    free(modes);
    if(mailbox && mode!=VK_PRESENT_MODE_MAILBOX_KHR) {
        fprintf(stderr,"MAILBOX available but blocking FIFO selected\n"); dc_gpu_destroy(g); return 5;
    }
    finite_acquire=true;idle_calls=0;
    for(unsigned frame=0;frame<90;++frame) {
        if(frame==30) SDL_HideWindow(g->window);
        if(frame==60) SDL_ShowWindow(g->window);
        presenting=true;
        bool okay=dc_gpu_present_chunks_steps(g,1,err,sizeof(err));
        presenting=false;
        if(!okay) { fprintf(stderr,"%s\n",err); return 6; }
        SDL_Delay(2);
    }
    bool okay=finite_acquire && idle_calls==0;
    fprintf(stdout,"mode=%d finite acquire=%d presentation queue-idle calls=%u\n",mode,finite_acquire,idle_calls);
    dc_gpu_destroy(g);
    return okay ? 0 : 7;
}

static void test_recreated_hidden_windows_keep_presenting_within_deadline(void) {
    pid_t child=fork(); ASSERT_TRUE(child>=0);
    if(child==0) {
        for(unsigned cycle=0;cycle<3;++cycle) { int result=window_cycle(); if(result) exit(result); }
        exit(0);
    }
    uint64_t start=SDL_GetTicks64(); int status=0;
    while(waitpid(child,&status,WNOHANG)==0) {
        if(SDL_GetTicks64()-start>15000) {
            kill(child,SIGKILL); waitpid(child,&status,0);
            ASSERT_TRUE(!"presentation exceeded 15-second watchdog");
        }
        SDL_Delay(10);
    }
    ASSERT_TRUE(WIFEXITED(status)); ASSERT_INT_EQ(WEXITSTATUS(status),0); PASS();
}
int main(void) {
    RUN(test_recreated_hidden_windows_keep_presenting_within_deadline);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail ? 1 : 0;
}
