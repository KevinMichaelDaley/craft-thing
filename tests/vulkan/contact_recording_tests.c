#include <stdint.h>
#include <stdio.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static unsigned dispatches,barriers,points[8];
static VkMemoryBarrier2 memory[8];
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint p,VkPipelineLayout l,uint32_t first,uint32_t count,const VkDescriptorSet *s,uint32_t dynamic_count,const uint32_t *offsets) {
    (void)c;(void)p;(void)l;(void)first;(void)count;(void)s;(void)dynamic_count;(void)offsets;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) {(void)c;(void)p;(void)pipeline;}
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags f,uint32_t o,uint32_t n,const void *v) {(void)c;(void)l;(void)f;(void)o;(void)n;(void)v;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;(void)x;(void)y;(void)z;dispatches++;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer c,VkBuffer b,VkDeviceSize o) {(void)c;(void)b;(void)o;dispatches++;}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *info) {
    (void)c;
    if(barriers<8&&info->memoryBarrierCount==1) {
        points[barriers]=dispatches;memory[barriers]=info->pMemoryBarriers[0];
    }
    barriers++;
}
static void recording_case(bool typed) {
    dispatches=barriers=0;
    dc_gpu_t gpu={.width=64,.height=64,.page_width=1,.page_height=1,.body_count=64};
    if(typed) gpu.contact_pixel_pipeline=(VkPipeline)(uintptr_t)4;
    dc_gpu_record_contacts(&gpu);
    printf("typed%d dispatches%u barriers%u\n",typed,dispatches,barriers);
    ASSERT_EQ(dispatches,typed?4u:3u);
    ASSERT_EQ(barriers,2u);
    ASSERT_EQ(points[0],1u);ASSERT_EQ(points[1],dispatches);
    ASSERT_TRUE((memory[0].srcAccessMask&VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)!=0);
    ASSERT_TRUE((memory[0].dstStageMask&VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT)!=0);
    ASSERT_TRUE((memory[0].dstAccessMask&VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)!=0);
    ASSERT_TRUE((memory[1].srcStageMask&VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)!=0);
    ASSERT_TRUE((memory[1].srcAccessMask&VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)!=0);
    ASSERT_TRUE((memory[1].dstStageMask&VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)!=0);
    ASSERT_TRUE((memory[1].dstAccessMask&VK_ACCESS_2_SHADER_STORAGE_READ_BIT)!=0);
    ASSERT_TRUE((memory[1].dstAccessMask&VK_ACCESS_2_HOST_READ_BIT)!=0);
    PASS();
}
static void test_atomic_candidate_passes_publish_together(void) {recording_case(true);}
static void test_generic_reference_keeps_both_dependency_boundaries(void) {recording_case(false);}
int main(void) {
    RUN(test_atomic_candidate_passes_publish_together);
    RUN(test_generic_reference_keeps_both_dependency_boundaries);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
