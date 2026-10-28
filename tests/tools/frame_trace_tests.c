#include "../../tools/diagnostics/frame_trace.c"

static int g_pass,g_fail;
static unsigned resets;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

VKAPI_ATTR VkResult VKAPI_CALL __real_vkBeginCommandBuffer(VkCommandBuffer command,
    const VkCommandBufferBeginInfo *begin) {(void)command;(void)begin;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(VkCommandBuffer command,VkQueryPool query,
    uint32_t first,uint32_t count) {(void)command;(void)query;(void)first;(void)count;resets++;}

static void test_frame_queries_survive_multiple_gpu_submissions(void) {
    dc_gpu_t g={0};g.command=(VkCommandBuffer)(uintptr_t)1;
    traced=&g;traced_primary=g.command;query_count=0;resets=0;
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ASSERT_EQ(__wrap_vkBeginCommandBuffer(g.command,&begin),VK_SUCCESS);
    ASSERT_EQ(resets,1u);
    query_count=2;
    ASSERT_EQ(__wrap_vkBeginCommandBuffer(g.command,&begin),VK_SUCCESS);
    ASSERT_EQ(query_count,2u);ASSERT_EQ(resets,1u);
    query_count=0;
    ASSERT_EQ(__wrap_vkBeginCommandBuffer(g.command,&begin),VK_SUCCESS);
    ASSERT_EQ(resets,2u);traced=NULL;PASS();
}
int main(void) {
    RUN(test_frame_queries_survive_multiple_gpu_submissions);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
