#define TRANSPORT_TEST_WIDTH 640
#define TRANSPORT_TEST_HEIGHT 16
#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"

static void test_dry_lines_skip_cache_setup_without_changing_inputs(void) {
    fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f&&initialize(&f->p));char err[256];
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};
    for(unsigned i=0;i<8;i++) ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,
        f->buffers+i,f->memory+i,(void **)(f->words+i),err,sizeof(err)));
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=i+1;
    uint64_t expected[8]={0};
    for(unsigned i=0;i<8;i++) for(unsigned j=0;j<sizes[i];j++) expected[i]+=f->words[i][j];
    VkBuffer profile;VkDeviceMemory profile_memory;uint32_t *counts;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,32,&profile,&profile_memory,(void **)&counts,err,sizeof(err)));
    uint32_t indices[]={1,2,3,8,10,28,29,32,31};
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,"build/diagnostics/fluid_dry_line_profile.comp.spv",indices,9,2));
    for(unsigned i=0;i<9;i++) {
        VkDescriptorBufferInfo buffer={i<8?f->buffers[i]:profile,0,i<8?sizes[i]*4u:32u};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
            .dstBinding=indices[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
        vkUpdateDescriptorSets(f->p.device,1,&write,0,NULL);
    }
    uint32_t loads[2]={0};bool exact=true;
    for(unsigned optimized=0;optimized<2;optimized++) {
        uint32_t values[]={2,optimized};VkSpecializationMapEntry entries[]={{0,0,4},{10,4,4}};
        VkSpecializationInfo spec={2,entries,sizeof(values),values};
        VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=f->p.shader,.pName="main",.pSpecializationInfo=&spec},.layout=f->p.layout};
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=VK_NULL_HANDLE;
        ASSERT_TRUE(vkCreateComputePipelines(f->p.device,VK_NULL_HANDLE,1,&info,NULL,&f->p.pipeline)==VK_SUCCESS);
        memset(counts,0,32);ASSERT_TRUE(begin(&f->p));
        vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
        vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
        uint32_t push[]={WIDTH,HEIGHT,2,0,0,bits(6.f),0,0};
        vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(f->p.command,HEIGHT,1,1);host_barrier(&f->p,1);
        ASSERT_TRUE(vkEndCommandBuffer(f->p.command)==VK_SUCCESS&&submit(&f->p,"dry line cache work",1));
        loads[optimized]=counts[7];
        for(unsigned i=0;i<8;i++) {uint64_t actual=0;for(unsigned j=0;j<sizes[i];j++) actual+=f->words[i][j];exact=exact&&actual==expected[i];}
    }
    printf("dry cache cell loads%u -> %u\n",loads[0],loads[1]);
    fixture_release_pipeline(&f->p);
    vkUnmapMemory(f->p.device,profile_memory);vkDestroyBuffer(f->p.device,profile,NULL);vkFreeMemory(f->p.device,profile_memory,NULL);
    for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_TRUE(exact);ASSERT_EQ(loads[0],CELLS);ASSERT_EQ(loads[1],0u);PASS();
}
int main(void) {RUN(test_dry_lines_skip_cache_setup_without_changing_inputs);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
