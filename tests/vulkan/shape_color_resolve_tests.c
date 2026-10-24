#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_staged_colors_preserve_both_banks_with_bounded_address_work(void) {
    dc_gpu_t *g=NULL;char err[256]={0};
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(g->runtime_body_consumers);
    uint32_t low=dc_gpu_pixel_pool_word_offset(g)+DC_GPU_PIXEL_POOL_HEADER_WORDS;
    uint32_t capacity=dc_gpu_pixel_pool_capacity(g),targets[]={low+capacity,low+1u,low+capacity+2u};
    float expected[3][32]={{0}};
    for(unsigned word=0;word<2;word++) {
        float *source=dc_gpu_shape_colors(g,word?low+capacity+1u:low);
        for(unsigned value=0;value<32;value++) expected[word][value]=(float)(word*32u+value+1u)/64.f;
        memcpy(source,expected[word],sizeof(expected[word]));
    }
    const uint32_t markers[]={0x80000001u,0x80000000u|(capacity+2u),0x80000000u};
    for(unsigned word=0;word<3;word++) {
        float *target=dc_gpu_shape_colors(g,targets[word]);memset(target,0,32u*sizeof(float));
        memcpy(target+3,markers+word,4);
    }
    VkBuffer profile=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void *mapped=NULL;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,16,&profile,&memory,&mapped,err,sizeof(err)));memset(mapped,0,16);
    VkDescriptorBufferInfo buffer={profile,0,16};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
        .dstBinding=31,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
    vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    VkShaderModule module=VK_NULL_HANDLE;
    ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/diagnostics/shape_color_resolve_probe.spv",&module,err,sizeof(err)));
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main"},.layout=g->pipeline_layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline),VK_SUCCESS);
    ASSERT_EQ(vkResetCommandBuffer(g->command,0),VK_SUCCESS);
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ASSERT_EQ(vkBeginCommandBuffer(g->command,&begin),VK_SUCCESS);
    VkMemoryBarrier2 barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_HOST_BIT,.srcAccessMask=VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&barrier};
    vkCmdPipelineBarrier2(g->command,&dependency);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    uint32_t push[]={64,64,low,0,capacity,0,0,0};
    vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,1,1,1);
    barrier.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;barrier.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    barrier.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;barrier.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(g->command,&dependency);
    ASSERT_EQ(vkEndCommandBuffer(g->command),VK_SUCCESS);
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    ASSERT_EQ(vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE),VK_SUCCESS);ASSERT_EQ(vkQueueWaitIdle(g->queue),VK_SUCCESS);
    unsigned errors=0;for(unsigned word=0;word<3;word++)
        errors+=memcmp(dc_gpu_shape_colors(g,targets[word]),expected[word],sizeof(expected[word]))!=0;
    uint32_t pool=((uint32_t *)mapped)[0],capacities=((uint32_t *)mapped)[1];
    printf("resolved3 words: RGBA errors%u pool queries%u capacity queries%u\n",errors,pool,capacities);
    vkDestroyPipeline(g->device,pipeline,NULL);vkDestroyShaderModule(g->device,module,NULL);
    vkUnmapMemory(g->device,memory);vkDestroyBuffer(g->device,profile,NULL);vkFreeMemory(g->device,memory,NULL);
    dc_gpu_destroy(g);ASSERT_EQ(errors,0u);ASSERT_TRUE(pool<=3u);ASSERT_TRUE(capacities<=3u);PASS();
}
int main(void) {
    RUN(test_staged_colors_preserve_both_banks_with_bounded_address_work);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
