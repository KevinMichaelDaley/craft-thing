#define TRANSPORT_TEST_WIDTH 640
#define TRANSPORT_TEST_HEIGHT 360
#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"

enum { SAMPLES=5 };
static int duration_order(const void *a,const void *b) {
    double x=*(const double *)a,y=*(const double *)b;
    return (x>y)-(x<y);
}

static bool measure(fixture_t *f,VkQueryPool pool,unsigned mode,float magnitude,double *result) {
    bool vertical=mode<2;
    float speed=(mode&1u)?-magnitude:magnitude;
    memset(f->words[4],0,sizes[4]*4);
    for(unsigned i=0;i<CELLS;i++) f->words[4][2*i+(vertical?1:0)]=bits(speed);
    if(!begin(&f->p)) return false;
    vkCmdResetQueryPool(f->p.command,pool,0,2*SAMPLES);
    VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT};
    vkCmdPipelineBarrier(f->p.command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,1,&memory,0,NULL,0,NULL);
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[]={WIDTH,HEIGHT,mode,0,0,bits(6.f),0,0};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    for(unsigned i=0;i<SAMPLES;i++) {
        vkCmdWriteTimestamp2(f->p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,2*i);
        vkCmdDispatch(f->p.command,vertical?WIDTH:HEIGHT,1,1);
        vkCmdWriteTimestamp2(f->p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,2*i+1);
        memory.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
        memory.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(f->p.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,1,&memory,0,NULL,0,NULL);
    }
    host_barrier(&f->p,1);
    if(vkEndCommandBuffer(f->p.command)!=VK_SUCCESS||!submit(&f->p,"no-transfer line work",1)) return false;
    uint64_t times[2*SAMPLES];
    if(vkGetQueryPoolResults(f->p.device,pool,0,2*SAMPLES,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)!=VK_SUCCESS) return false;
    double samples[SAMPLES];
    for(unsigned i=0;i<SAMPLES;i++) samples[i]=(times[2*i+1]-times[2*i])*f->gpu.timestamp_period/1e6;
    qsort(samples,SAMPLES,sizeof(*samples),duration_order);*result=samples[SAMPLES/2];return true;
}

static void test_full_recipient_work_is_close_to_zero_velocity_work(void) {
    fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f&&initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(f->p.physical,&properties);
    f->gpu.timestamp_period=properties.limits.timestampPeriod;
    for(unsigned i=0;i<7;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
        memset(f->words[i],0,sizes[i]*4);
    }
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=i+1;
    for(unsigned i=0;i<CHUNKS*4096;i++) f->words[0][2*i+1]=FULL;
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,"build/shaders/fluid_transport.comp.spv",bindings,7,0));
    VkDescriptorBufferInfo buffers[7];VkWriteDescriptorSet writes[7];
    for(unsigned i=0;i<7;i++) {
        buffers[i]=(VkDescriptorBufferInfo){f->buffers[i],0,sizes[i]*4};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[i]};
    }
    vkUpdateDescriptorSets(f->p.device,7,writes,0,NULL);
    VkQueryPool pool;VkQueryPoolCreateInfo query={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType=VK_QUERY_TYPE_TIMESTAMP,.queryCount=2*SAMPLES};
    ASSERT_EQ(vkCreateQueryPool(f->p.device,&query,NULL,&pool),VK_SUCCESS);
    bool bounded=true;
    for(unsigned mode=0;mode<4;mode++) {
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=VK_NULL_HANDLE;
        uint32_t values[]={mode,mode<2?HEIGHT:WIDTH};
        VkSpecializationMapEntry entries[]={{.constantID=0,.size=4},{.constantID=1,.offset=4,.size=4}};
        VkSpecializationInfo constants={.mapEntryCount=2,.pMapEntries=entries,.dataSize=sizeof(values),.pData=values};
        VkComputePipelineCreateInfo pipeline={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
                .module=f->p.shader,.pName="main",.pSpecializationInfo=&constants},.layout=f->p.layout};
        ASSERT_EQ(vkCreateComputePipelines(f->p.device,VK_NULL_HANDLE,1,&pipeline,NULL,&f->p.pipeline),VK_SUCCESS);
        double quiet,full,long_full;
        ASSERT_TRUE(measure(f,pool,mode,0,&quiet));
        ASSERT_TRUE(measure(f,pool,mode,.125f,&full));
        ASSERT_TRUE(measure(f,pool,mode,8.f,&long_full));
        printf("mode%u zero speed %.3f ms, full recipient %.3f ms, long full %.3f ms, ratios %.3f/%.3f\n",mode,quiet,full,long_full,full/quiet,long_full/quiet);
        bounded=bounded&&full<=quiet*1.25+.05&&long_full<=quiet*1.25+.05;
    }
    bool exact=true;
    for(unsigned i=0;i<CHUNKS*4096;i++) exact=exact&&f->words[0][2*i+1]==FULL;
    vkDestroyQueryPool(f->p.device,pool,NULL);fixture_release_pipeline(&f->p);
    for(unsigned i=0;i<7;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_TRUE(exact);ASSERT_TRUE(bounded);PASS();
}

int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_full_recipient_work_is_close_to_zero_velocity_work);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
