#include <stdint.h>
#include <stdio.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static uint32_t phase=UINT32_MAX,solver_barriers,host_publications,bad_dependencies;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint p,VkPipelineLayout l,uint32_t first,uint32_t count,const VkDescriptorSet *s,uint32_t dynamic_count,const uint32_t *offsets) {
    (void)c;(void)p;(void)l;(void)first;(void)count;(void)s;(void)dynamic_count;(void)offsets;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) {
    (void)c;(void)p;uintptr_t value=(uintptr_t)pipeline;
    phase=value>=1&&value<=DC_GPU_SOLVER_PHASE_COUNT?(uint32_t)value-1u:UINT32_MAX;
}
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags f,uint32_t o,uint32_t n,const void *v) {(void)c;(void)l;(void)f;(void)o;(void)n;(void)v;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;(void)x;(void)y;(void)z;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer c,VkBuffer b,VkDeviceSize o) {(void)c;(void)b;(void)o;}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *info) {
    (void)c;if(phase==UINT32_MAX) return;
    for(unsigned i=0;i<info->memoryBarrierCount;i++) {
        const VkMemoryBarrier2 *m=info->pMemoryBarriers+i;solver_barriers++;
        bool host=(m->dstStageMask&VK_PIPELINE_STAGE_2_HOST_BIT)!=0u;
        if(host) {host_publications++;if(phase!=11u) bad_dependencies++;}
        if(host!=((m->dstAccessMask&VK_ACCESS_2_HOST_READ_BIT)!=0u)) bad_dependencies++;
        if(!(m->srcStageMask&VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)||
           !(m->srcAccessMask&VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)||
           !(m->dstStageMask&VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)||
           !(m->dstStageMask&VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT)||
           !(m->dstAccessMask&VK_ACCESS_2_SHADER_STORAGE_READ_BIT)||
           !(m->dstAccessMask&VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)||
           !(m->dstAccessMask&VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)) bad_dependencies++;
    }
}
static void test_solver_publishes_to_host_once_after_all_iterations(void) {
    dc_gpu_t gpu={.width=64,.height=64,.page_width=1,.page_height=1,.body_count=1};
    for(unsigned i=0;i<DC_GPU_SOLVER_PHASE_COUNT;i++) gpu.solver_pipelines[i]=(VkPipeline)(uintptr_t)(i+1u);
    dc_gpu_record_solver(&gpu);
    printf("solver barriers%u host publications%u invalid dependencies%u\n",solver_barriers,host_publications,bad_dependencies);
    ASSERT_TRUE(solver_barriers>200u);
    ASSERT_EQ(host_publications,1u);ASSERT_EQ(bad_dependencies,0u);PASS();
}
int main(void) {
    RUN(test_solver_publishes_to_host_once_after_all_iterations);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
