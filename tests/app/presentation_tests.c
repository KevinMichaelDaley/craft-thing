#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ferrum/fire/gas_gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "../test_window.h"

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
static bool reject_next_acquire, reject_next_present;
static unsigned present_calls;
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
    if (reject_next_acquire) {
        reject_next_acquire = false;
        return VK_TIMEOUT;
    }
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
    ++present_calls;
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
    if (reject_next_present) {
        /* Complete the real queue operation so the test can use this swapchain again. */
        reject_next_present = false;
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
            return VK_ERROR_OUT_OF_DATE_KHR;
    }
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
    uint64_t start=SDL_GetPerformanceCounter();
    if(!dc_test_create_window(&g,64,64,256,256,"build/shaders/pattern.comp.spv",err,sizeof(err))) {
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
    uint64_t first_frame=SDL_GetPerformanceCounter();
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
    uint64_t last_frame=SDL_GetPerformanceCounter(),hz=SDL_GetPerformanceFrequency();
    fprintf(stdout,"Brush window setup %.3f ms, displayed frame mean %.3f ms over 90 frames\n",
        1000.0*(first_frame-start)/hz,1000.0*(last_frame-first_frame)/(90.0*hz));
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

static dc_gpu_t *progress_window(char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool ready = dc_test_create_window(&gpu, 64, 64, 256, 256,
                        "build/shaders/pattern.comp.spv", err, cap) &&
                 dc_gpu_upload_chunk(gpu, 0, chunk, err, cap) &&
                 dc_gpu_set_page(gpu, 0, 0, 0, err, cap);
    free(chunk);
    if (ready) return gpu;
    fprintf(stderr, "Presentation fixture: %s\n", err);
    dc_gpu_destroy(gpu);
    return NULL;
}

static void test_combined_tick_progress_follows_fence_completion(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = progress_window(err, sizeof(err));
    ASSERT_TRUE(gpu);
    dc_gpu_body_t body = {.id=1, .active=1, .x_fp=10*65536, .y_fp=10*65536,
                         .vx_fp=4096, .width=4, .height=4};
    int32_t initial_x = body.x_fp, velocity = body.vx_fp;
    bool okay = dc_gpu_set_rigid_solver(gpu, true) &&
                dc_gpu_spawn_body(gpu, body, err, sizeof(err)) &&
                dc_gpu_set_body_motion(gpu, 1, 0, 0,
                    DC_GPU_BODY_KINEMATIC | DC_GPU_BODY_LOCK_ROTATION, err, sizeof(err));
    uint32_t before = UINT32_MAX, completed = UINT32_MAX, retry = UINT32_MAX;
    uint32_t success = UINT32_MAX;
    int32_t before_x = 0, completed_x = 0, retried_x = 0, success_x = 0;
    present_calls = 0;
    reject_next_acquire = okay;
    okay = okay && !dc_gpu_present_chunks_steps_progress(gpu, 2, &before, err, sizeof(err)) &&
           dc_gpu_read_body_id(gpu, 1, &body, err, sizeof(err));
    before_x = body.x_fp;
    reject_next_present = okay;
    okay = okay && !dc_gpu_present_chunks_steps_progress(gpu, 2, &completed, err, sizeof(err)) &&
           dc_gpu_read_body_id(gpu, 1, &body, err, sizeof(err));
    completed_x = body.x_fp;
    okay = okay && dc_gpu_present_chunks_steps_progress(gpu, 0, &retry, err, sizeof(err)) &&
           dc_gpu_read_body_id(gpu, 1, &body, err, sizeof(err));
    retried_x = body.x_fp;
    okay = okay && dc_gpu_present_chunks_steps_progress(gpu, 2, &success, err, sizeof(err)) &&
           dc_gpu_read_body_id(gpu, 1, &body, err, sizeof(err));
    success_x = body.x_fp;
    uint32_t validation_errors = dc_gpu_validation_errors(gpu);
    dc_gpu_destroy(gpu);
    reject_next_acquire = reject_next_present = false;
    if (!okay) fprintf(stderr, "Combined frame: %s\n", err);
    ASSERT_TRUE(okay);
    printf("Combined progress before acquire %u, after failed present %u, retry %u, success %u\n",
           before, completed, retry, success);
    ASSERT_EQ(before, 0u);
    ASSERT_EQ(completed, 2u);
    ASSERT_EQ(retry, 0u);
    ASSERT_EQ(success, 2u);
    ASSERT_EQ(present_calls, 3u);
    ASSERT_EQ(before_x, initial_x);
    ASSERT_EQ(completed_x, initial_x + 2 * velocity);
    ASSERT_EQ(retried_x, completed_x);
    ASSERT_EQ(success_x, initial_x + 4 * velocity);
    ASSERT_EQ(validation_errors, 0u);
    PASS();
}

static void test_gas_tick_progress_survives_acquire_timeout(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = progress_window(err, sizeof(err));
    ASSERT_TRUE(gpu);
    const fr_gas_model_t gas = {.gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.};
    fr_gas_gpu_clock_t before, after, paused;
    bool okay = fr_gas_gpu_enable(gpu, &gas, .0012f, 293.15f, err, sizeof(err)) &&
                dc_gpu_set_tick_seconds(gpu, 1.f / 60.f) && fr_gas_gpu_clock(gpu, &before);
    uint32_t completed = UINT32_MAX, retry = UINT32_MAX;
    present_calls = 0;
    reject_next_acquire = okay;
    okay = okay && !dc_gpu_present_chunks_steps_progress(gpu, 2, &completed, err, sizeof(err)) &&
           fr_gas_gpu_clock(gpu, &after) &&
           dc_gpu_present_chunks_steps_progress(gpu, 0, &retry, err, sizeof(err)) &&
           fr_gas_gpu_clock(gpu, &paused);
    uint32_t validation_errors = dc_gpu_validation_errors(gpu);
    dc_gpu_destroy(gpu);
    reject_next_acquire = false;
    if (!okay) fprintf(stderr, "Gas frame: %s\n", err);
    ASSERT_TRUE(okay);
    printf("Gas acquire timeout completed %u ticks, retry completed %u\n", completed, retry);
    ASSERT_EQ(completed, 2u);
    ASSERT_EQ(retry, 0u);
    ASSERT_EQ(after.physical_steps - before.physical_steps, 2u);
    ASSERT_EQ(paused.physical_steps, after.physical_steps);
    ASSERT_EQ(paused.advanced_seconds, after.advanced_seconds);
    ASSERT_EQ(present_calls, 1u);
    ASSERT_EQ(validation_errors, 0u);
    PASS();
}

int main(void) {
    RUN(test_combined_tick_progress_follows_fence_completion);
    RUN(test_gas_tick_progress_survives_acquire_timeout);
    if (!getenv("DC_TEST_PRESENT_PROGRESS_ONLY")) {
        RUN(test_recreated_hidden_windows_keep_presenting_within_deadline);
    }
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail ? 1 : 0;
}
