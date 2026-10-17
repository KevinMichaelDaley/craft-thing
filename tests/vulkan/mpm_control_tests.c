#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_changed_components_keep_work_under_a_legal_lane_schedule(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical,.width=64,.height=64};
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,39u*4u,&p.buffer,&p.memory,(void **)&p.mapped,err,sizeof(err)));
    for(unsigned i=0;i<39;i++) p.mapped[i]=4369;
    p.mapped[0]=7;p.mapped[35]=1;
    const uint32_t bindings[]={1,2,27,28,29,30};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,VK_NULL_HANDLE,"build/diagnostics/mpm_control_schedule.spv",bindings,6,UINT32_MAX));
    ASSERT_TRUE(begin(&p));vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    uint32_t push[]={64,64,5,0,0,0,0};
    vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);
    ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);ASSERT_TRUE(submit(&p,"convergence control schedule",1));
    uint32_t actual=p.mapped[36];bool clean=p.mapped[35]==0;
    printf("changed component count after staggered lanes: %u expected7\n",actual);
    for(unsigned i=0;i<39;i++) if(i!=0&&i!=35&&i!=36) clean=clean&&p.mapped[i]==4369;
    fixture_release_pipeline(&p);vkUnmapMemory(p.device,p.memory);vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(clean);ASSERT_EQ(actual,7u);PASS();
}
int main(void) {
    RUN(test_changed_components_keep_work_under_a_legal_lane_schedule);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
