#define main pressure_stencil_probe_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_every_aperture_and_cached_payload_round_trip_without_extra_words(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));ASSERT_TRUE(allocate_buffer(&p));
    ASSERT_TRUE(create_pipeline(&p,"build/diagnostics/pressure_stencil_pack_probe.spv"));
    memset(p.mapped,0,PROBE_WORDS*sizeof(uint32_t));ASSERT_TRUE(begin(&p));
    VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,1,&memory,0,NULL,0,NULL);
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);
    ASSERT_TRUE(check(vkEndCommandBuffer(p.command),"pack probe command end"));
    ASSERT_TRUE(submit(&p,"exact pressure packet",1));
    uint32_t result[5];memcpy(result,p.mapped,sizeof(result));
    printf("aperture cases %u: coefficient errors %u payload errors %u validity errors %u low-byte errors %u\n",
        result[4],result[0],result[1],result[2],result[3]);
    vkDestroyPipeline(p.device,p.pipeline,NULL);vkDestroyShaderModule(p.device,p.shader,NULL);
    vkDestroyPipelineLayout(p.device,p.layout,NULL);vkDestroyDescriptorPool(p.device,p.descriptor_pool,NULL);
    vkDestroyDescriptorSetLayout(p.device,p.descriptor_layout,NULL);vkUnmapMemory(p.device,p.memory);
    vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);
    vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_EQ(result[4],65537u);for(unsigned i=0;i<4;i++) ASSERT_EQ(result[i],0u);PASS();
}
int main(void) {
    RUN(test_every_aperture_and_cached_payload_round_trip_without_extra_words);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
