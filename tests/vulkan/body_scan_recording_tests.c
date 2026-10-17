#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/body_scan_gpu.h"
static int g_pass,g_fail;
static uint32_t dispatched,barriers,pushes,counts[16];
static VkPipeline bound[16],pipeline;
static uint64_t addresses[16];
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint b,VkPipeline p) {(void)c;(void)b;pipeline=p;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags s,uint32_t o,uint32_t n,const void *p) {
    (void)c;(void)l;(void)s;(void)o;uint32_t words[6]={0};
    if(n==sizeof(words)&&pushes<16) {memcpy(words,p,n);counts[pushes]=words[4];addresses[pushes]=(uint64_t)words[0]|(uint64_t)words[1]<<32;pushes++;}
}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;(void)x;(void)y;(void)z;if(dispatched<16) bound[dispatched++]=pipeline;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *d) {(void)c;(void)d;barriers++;}
static void test_hierarchical_recording_scans_up_then_adds_parent_prefixes_down(void) {
    dc_gpu_body_scan_plan_t plan={0};char err[128];
    ASSERT_TRUE(dc_gpu_body_scan_plan_create(16385,65535,65535,&plan,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_body_scan_record((VkCommandBuffer)(uintptr_t)1,&plan,0x100000000ull,plan.bytes,
        (VkPipelineLayout)(uintptr_t)2,(VkPipeline)(uintptr_t)3,(VkPipeline)(uintptr_t)4));
    const uint32_t expected[]={16385,129,2,129,16385};
    ASSERT_EQ(dispatched,5u);ASSERT_EQ(pushes,5u);ASSERT_TRUE(barriers>=5u);
    for(uint32_t i=0;i<5;i++) {ASSERT_EQ(counts[i],expected[i]);ASSERT_EQ(bound[i],(VkPipeline)(uintptr_t)(i<3?3:4));}
    ASSERT_EQ(addresses[0],0x100000000ull);ASSERT_EQ(addresses[4],addresses[0]);
    dc_gpu_body_scan_plan_destroy(&plan);PASS();
}
static void test_short_scratch_is_rejected_before_any_gpu_commands(void) {
    dc_gpu_body_scan_plan_t plan={0};char err[128];
    ASSERT_TRUE(dc_gpu_body_scan_plan_create(129,65535,65535,&plan,err,sizeof(err)));
    uint32_t before=dispatched;
    ASSERT_TRUE(!dc_gpu_body_scan_record((VkCommandBuffer)(uintptr_t)1,&plan,0x100000000ull,plan.bytes-1,
        (VkPipelineLayout)(uintptr_t)2,(VkPipeline)(uintptr_t)3,(VkPipeline)(uintptr_t)4));
    ASSERT_EQ(dispatched,before);dc_gpu_body_scan_plan_destroy(&plan);PASS();
}
int main(void) {RUN(test_hierarchical_recording_scans_up_then_adds_parent_prefixes_down);RUN(test_short_scratch_is_rejected_before_any_gpu_commands);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
