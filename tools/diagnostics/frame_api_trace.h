#ifndef DC_DIAGNOSTIC_FRAME_API_TRACE_H
#define DC_DIAGNOSTIC_FRAME_API_TRACE_H
#ifdef __linux__
#include <execinfo.h>
#endif

enum { API_COMPILE,API_ALLOCATE,API_ACQUIRE,API_PRESENT,API_FENCE,API_IDLE,API_SERVICE,API_COUNT };
static double frame_api_ms[API_COUNT];
static unsigned frame_api_number;
static void frame_api_idle_stack(double elapsed) {
#ifdef __linux__
    if(frame_api_number>12u&&elapsed>1.&&getenv("DC_FRAME_TRACE_IDLE_STACKS")) {
        void *stack[12];int count=backtrace(stack,12);
        fprintf(stderr,"idle-stack after-frame%u %.3fms\n",frame_api_number,elapsed);
        backtrace_symbols_fd(stack,count,2);
    }
#else
    (void)elapsed;
#endif
}
static double frame_api_clock(void) {
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return now.tv_sec+now.tv_nsec/1e9;
}
VkResult __real_vkCreateComputePipelines(VkDevice,VkPipelineCache,uint32_t,const VkComputePipelineCreateInfo *,const VkAllocationCallbacks *,VkPipeline *);
VkResult __wrap_vkCreateComputePipelines(VkDevice d,VkPipelineCache c,uint32_t n,const VkComputePipelineCreateInfo *i,const VkAllocationCallbacks *a,VkPipeline *p) {
    double start=frame_api_clock();VkResult r=__real_vkCreateComputePipelines(d,c,n,i,a,p);
    frame_api_ms[API_COMPILE]+=(frame_api_clock()-start)*1000.;return r;
}
VkResult __real_vkAllocateMemory(VkDevice,const VkMemoryAllocateInfo *,const VkAllocationCallbacks *,VkDeviceMemory *);
VkResult __wrap_vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo *i,const VkAllocationCallbacks *a,VkDeviceMemory *m) {
    double start=frame_api_clock();VkResult r=__real_vkAllocateMemory(d,i,a,m);
    frame_api_ms[API_ALLOCATE]+=(frame_api_clock()-start)*1000.;return r;
}
VkResult __real_vkAcquireNextImageKHR(VkDevice,VkSwapchainKHR,uint64_t,VkSemaphore,VkFence,uint32_t *);
VkResult __wrap_vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t t,VkSemaphore sem,VkFence f,uint32_t *i) {
    double start=frame_api_clock();VkResult r=__real_vkAcquireNextImageKHR(d,s,t,sem,f,i);
    frame_api_ms[API_ACQUIRE]+=(frame_api_clock()-start)*1000.;return r;
}
VkResult __real_vkQueuePresentKHR(VkQueue,const VkPresentInfoKHR *);
VkResult __wrap_vkQueuePresentKHR(VkQueue q,const VkPresentInfoKHR *i) {
    double start=frame_api_clock();VkResult r=__real_vkQueuePresentKHR(q,i);
    frame_api_ms[API_PRESENT]+=(frame_api_clock()-start)*1000.;return r;
}
VkResult __real_vkWaitForFences(VkDevice,uint32_t,const VkFence *,VkBool32,uint64_t);
VkResult __wrap_vkWaitForFences(VkDevice d,uint32_t n,const VkFence *f,VkBool32 all,uint64_t t) {
    double start=frame_api_clock();VkResult r=__real_vkWaitForFences(d,n,f,all,t);
    frame_api_ms[API_FENCE]+=(frame_api_clock()-start)*1000.;return r;
}
VkResult __real_vkQueueWaitIdle(VkQueue);
VkResult __wrap_vkQueueWaitIdle(VkQueue q) {
    double start=frame_api_clock();VkResult r=__real_vkQueueWaitIdle(q);
    double elapsed=(frame_api_clock()-start)*1000.;frame_api_ms[API_IDLE]+=elapsed;
    frame_api_idle_stack(elapsed);return r;
}
VkResult __real_vkDeviceWaitIdle(VkDevice);
VkResult __wrap_vkDeviceWaitIdle(VkDevice d) {
    double start=frame_api_clock();VkResult r=__real_vkDeviceWaitIdle(d);
    double elapsed=(frame_api_clock()-start)*1000.;frame_api_ms[API_IDLE]+=elapsed;
    frame_api_idle_stack(elapsed);return r;
}
bool __real_dc_gpu_runtime_bodies_service(dc_gpu_t *,char *,uint32_t);
bool __wrap_dc_gpu_runtime_bodies_service(dc_gpu_t *g,char *err,uint32_t cap) {
    double start=frame_api_clock();bool okay=__real_dc_gpu_runtime_bodies_service(g,err,cap);
    frame_api_ms[API_SERVICE]+=(frame_api_clock()-start)*1000.;return okay;
}
static void frame_api_print(unsigned number) {
    frame_api_number=number;
    printf("frame-api,%u,compile%.3f,allocation%.3f,acquire%.3f,present%.3f,fence%.3f,idle%.3f,body-service%.3f\n",
        number,frame_api_ms[API_COMPILE],frame_api_ms[API_ALLOCATE],frame_api_ms[API_ACQUIRE],
        frame_api_ms[API_PRESENT],frame_api_ms[API_FENCE],frame_api_ms[API_IDLE],frame_api_ms[API_SERVICE]);
    memset(frame_api_ms,0,sizeof(frame_api_ms));
}
#endif
