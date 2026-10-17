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

static void control_case(bool changed,bool two_dimensional) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    uint32_t width=two_dimensional?4097u:64u,height=width,tiles=((width+15u)/16u)*((height+15u)/16u);
    uint32_t control=3u+2u*tiles,words=control+4u,x=two_dimensional?33025u:7u,y=two_dimensional?2u:1u;
    dc_gpu_t gpu={.device=p.device,.physical=p.physical,.width=width,.height=height};
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,words*4u,&p.buffer,&p.memory,(void **)&p.mapped,err,sizeof(err)));
    for(unsigned i=0;i<words;i++) p.mapped[i]=4369;
    p.mapped[0]=x;p.mapped[1]=y;p.mapped[2]=1;p.mapped[control]=changed;
    p.mapped[control+2u]=p.mapped[control+3u]=1;
    const uint32_t bindings[]={1,2,27,28,29,30};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,VK_NULL_HANDLE,"build/diagnostics/mpm_control_schedule.spv",bindings,6,UINT32_MAX));
    VkDescriptorBufferInfo activity={p.buffer,0,words*4u};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,.dstBinding=27,
        .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&activity};
    vkUpdateDescriptorSets(p.device,1,&write,0,NULL);
    ASSERT_TRUE(begin(&p));vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    uint32_t push[]={width,height,5,0,0,0,0};
    vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);
    ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);ASSERT_TRUE(submit(&p,"convergence control schedule",1));
    uint32_t actual=p.mapped[control+1u],expected=changed?x:0u;bool clean=p.mapped[control]==0;
    printf("changed%d component extent after staggered lanes: %u,%u,%u expected%u,%u,1\n",changed,
        actual,p.mapped[control+2u],p.mapped[control+3u],expected,changed?y:1u);
    clean=clean&&p.mapped[0]==x&&p.mapped[1]==y&&p.mapped[2]==1u&&
        p.mapped[control+2u]==(changed?y:1u)&&p.mapped[control+3u]==1u;
    for(unsigned i=3;i<words;i++) if(i<control||i>control+3u) clean=clean&&p.mapped[i]==4369;
    fixture_release_pipeline(&p);vkUnmapMemory(p.device,p.memory);vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(clean);ASSERT_EQ(actual,expected);PASS();
}
static void test_changed_components_keep_work_under_a_legal_lane_schedule(void) {control_case(true,false);}
static void test_unchanged_components_disable_all_work(void) {control_case(false,false);}
static void test_changed_components_preserve_two_dimensional_extent(void) {control_case(true,true);}
int main(void) {
    RUN(test_changed_components_keep_work_under_a_legal_lane_schedule);
    RUN(test_unchanged_components_disable_all_work);
    RUN(test_changed_components_preserve_two_dimensional_extent);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
