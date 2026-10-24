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
static bool painting;
static unsigned brush_fence_waits;
static double acquire_ms, enqueue_ms, render_wait_ms;
static VkSemaphore image_sems[32];
static bool image_sems_safe;
static dc_gpu_t *window_gpu;
static VkImage acquired_image;
static bool acquire_dependency;
static bool present_dependency;
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
    uint64_t start=SDL_GetPerformanceCounter();
    VkResult result=__real_vkAcquireNextImageKHR(d,s,t,sem,f,i);
    if (result==VK_SUCCESS && window_gpu && s==window_gpu->swapchain)
        acquired_image=window_gpu->swap_images[*i];
    acquire_ms+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
    return result;
}
void __real_vkCmdPipelineBarrier2(VkCommandBuffer,const VkDependencyInfo *);
void __wrap_vkCmdPipelineBarrier2(VkCommandBuffer command,const VkDependencyInfo *dependency) {
    for(uint32_t i=0;i<dependency->imageMemoryBarrierCount;i++) {
        const VkImageMemoryBarrier2 *barrier=dependency->pImageMemoryBarriers+i;
        if(barrier->image==acquired_image && barrier->oldLayout==VK_IMAGE_LAYOUT_UNDEFINED &&
           barrier->newLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            acquire_dependency=acquire_dependency &&
                (barrier->srcStageMask&VK_PIPELINE_STAGE_2_TRANSFER_BIT)!=0;
        if(barrier->image==acquired_image && barrier->newLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            present_dependency=present_dependency &&
                (barrier->dstStageMask&VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT)!=0;
    }
    __real_vkCmdPipelineBarrier2(command,dependency);
}
VkResult __real_vkQueuePresentKHR(VkQueue, const VkPresentInfoKHR *);
VkResult __wrap_vkQueuePresentKHR(VkQueue q,const VkPresentInfoKHR *p) {
    uint32_t image=p->pImageIndices[0]; VkSemaphore sem=p->pWaitSemaphores[0];
    if(image>=32) image_sems_safe=false;
    else {
        if(image_sems[image] && image_sems[image]!=sem) image_sems_safe=false;
        for(uint32_t i=0;i<32;++i) if(i!=image && image_sems[i]==sem) image_sems_safe=false;
        image_sems[image]=sem;
    }
    uint64_t start=SDL_GetPerformanceCounter();
    VkResult result=__real_vkQueuePresentKHR(q,p);
    enqueue_ms+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
    return result;
}
VkResult __real_vkWaitForFences(VkDevice,uint32_t,const VkFence *,VkBool32,uint64_t);
VkResult __wrap_vkWaitForFences(VkDevice d,uint32_t n,const VkFence *f,VkBool32 all,uint64_t timeout) {
    if(painting) brush_fence_waits++;
    uint64_t start=SDL_GetPerformanceCounter();
    VkResult result=__real_vkWaitForFences(d,n,f,all,timeout);
    render_wait_ms+=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
    return result;
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
    window_gpu=g;
    acquired_image=VK_NULL_HANDLE;
    acquire_dependency=true;
    present_dependency=true;
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
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk)); if(!chunk) return 8;
    bool uploaded=dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)); free(chunk);
    dc_gpu_body_t body={.id=1,.active=1,.x_fp=10*65536,.y_fp=10*65536,
        .vx_fp=4096,.width=4,.height=4};
    if(!uploaded || !dc_gpu_set_page(g,0,0,0,err,sizeof(err)) ||
       !dc_gpu_set_rigid_solver(g,true) || !dc_gpu_spawn_body(g,body,err,sizeof(err)) ||
       !dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC|DC_GPU_BODY_LOCK_ROTATION,err,sizeof(err))) return 9;
    finite_acquire=true;idle_calls=0;image_sems_safe=true;
    if(!dc_gpu_paint_material(g,63,63,0,DC_MATERIAL_AIR,err,sizeof(err))) return 10;
    idle_calls=brush_fence_waits=0;
    acquire_ms=enqueue_ms=render_wait_ms=0;
    for(uint32_t i=0;i<32;++i) image_sems[i]=VK_NULL_HANDLE;
    for(unsigned frame=0;frame<90;++frame) {
        if(frame==30) SDL_HideWindow(g->window);
        if(frame==60) SDL_ShowWindow(g->window);
        presenting=true;
        painting=true;
        bool painted=dc_gpu_paint_material(g,63,63,0,
            frame%2u?DC_MATERIAL_AIR:DC_MATERIAL_STONE,err,sizeof(err));
        painting=false;
        if(!painted) {fprintf(stderr,"%s\n",err);return 11;}
        bool okay=dc_gpu_present_chunks_steps(g,1,err,sizeof(err));
        presenting=false;
        if(!okay) { fprintf(stderr,"%s\n",err); return 6; }
        SDL_Delay(2);
    }
    bool okay=finite_acquire && idle_calls==0 && brush_fence_waits==90u && image_sems_safe && acquire_dependency && present_dependency;
    okay=okay && dc_gpu_read_body_id(g,1,&body,err,sizeof(err)) && body.x_fp==10*65536+90*4096;
    fprintf(stdout,"mode=%d finite acquire=%d presentation queue-idle calls=%u\n",mode,finite_acquire,idle_calls);
    fprintf(stdout,"90 brush transactions fence waits=%u\n",brush_fence_waits);
    fprintf(stdout,"90-frame waits: acquire=%.3fms enqueue=%.3fms render=%.3fms image semaphores safe=%d\n",
        acquire_ms,enqueue_ms,render_wait_ms,image_sems_safe);
    fprintf(stdout,"image transitions chain transfer wait=%d and present signal=%d\n",acquire_dependency,present_dependency);
    window_gpu=NULL;
    dc_gpu_destroy(g);
    return okay ? 0 : 7;
}

static void test_recreated_hidden_windows_keep_presenting_within_deadline(void) {
    fflush(stdout); pid_t child=fork(); ASSERT_TRUE(child>=0);
    if(child==0) {
        for(unsigned cycle=0;cycle<3;++cycle) { int result=window_cycle(); if(result) exit(result); }
        exit(0);
    }
    uint64_t start=SDL_GetTicks64(); int status=0; pid_t waited;
    while((waited=waitpid(child,&status,WNOHANG))==0) {
        if(SDL_GetTicks64()-start>15000) {
            kill(child,SIGKILL); waitpid(child,&status,0);
            ASSERT_TRUE(!"presentation exceeded 15-second watchdog");
        }
        SDL_Delay(10);
    }
    ASSERT_EQ(waited,child); ASSERT_TRUE(WIFEXITED(status)); ASSERT_INT_EQ(WEXITSTATUS(status),0); PASS();
}
int main(void) {
    RUN(test_recreated_hidden_windows_keep_presenting_within_deadline);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail ? 1 : 0;
}
