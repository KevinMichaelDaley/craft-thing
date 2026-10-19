#define TRANSPORT_TEST_WIDTH 640
#define TRANSPORT_TEST_HEIGHT 16
#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"

enum { PROFILE_WORDS=7,MAX_REQUESTS_PER_SOURCE=64 };

static unsigned seed_horizontal_cache(fixture_t *f) {
    unsigned leaves=1;
    while(leaves<(WIDTH+15u)/16u) leaves*=2u;
    for(unsigned y=0;y<HEIGHT;y++) {
        unsigned base=TREE_BASE+y*2u*leaves;
        for(unsigned tile=0;tile<leaves;tile++) {
            uint32_t value=FULL;
            for(unsigned k=0;k<16;k++) {
                unsigned x=tile*16+k;if(x>=WIDTH) continue;
                value=minimum(value,aperture(f,(int)x,(int)y,false));
                f->words[5][2u*(y*WIDTH+x)]=value;
            }
            f->words[6][base+leaves+tile]=value;value=FULL;
            for(int k=15;k>=0;k--) {
                unsigned x=tile*16+(unsigned)k;if(x>=WIDTH) continue;
                value=minimum(value,aperture(f,(int)x,(int)y,false));
                f->words[5][2u*(y*WIDTH+x)+1u]=value;
            }
        }
        for(unsigned node=leaves-1;node;node--)
            f->words[6][base+node]=minimum(f->words[6][base+2*node],f->words[6][base+2*node+1]);
    }
    return leaves;
}

static void partial_case(unsigned mode) {
    fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f&&initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(f->p.physical,&properties);
    for(unsigned i=0;i<8;i++)
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=i+1;
    uint64_t before=0;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        unsigned index=y*WIDTH+x,a=atlas(f,(int)x,(int)y);
        uint32_t material=x==0||x+1==WIDTH?1u:0u,water=material?0u:FULL/2u;
        f->words[0][2*a]=material;f->words[0][2*a+1]=water;
        f->expected[index]=water;before+=water;
        f->words[2][2*CELLS+index]=FULL/4u;
        f->words[2][3*CELLS+index]=FULL/2u;
        f->words[4][2*index]=bits((mode==2?1.f:-1.f)*WIDTH/6.f);
#ifdef DC_COLOR_CACHE_PROFILE
        float *colors=(float *)f->words[7];
        colors[4*a]=(float)((x*17u+y)%23u)/23.f;
        colors[4*a+1]=(float)((x+y*13u)%31u)/31.f;
        colors[4*a+2]=(float)((x*7u+y*5u)%37u)/37.f;colors[4*a+3]=1.f;
#endif
    }
    unsigned leaves=seed_horizontal_cache(f);
    for(unsigned y=0;y<HEIGHT;y++)
        for(int x=mode==2?WIDTH-2:1;x>=0&&x<WIDTH;x+=mode==2?-1:1)
            reference_move(f,x,(int)y,mode==2?1:-1,0);
    VkBuffer profile_buffer;VkDeviceMemory profile_memory;uint32_t *profile;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,PROFILE_WORDS*4,&profile_buffer,&profile_memory,(void **)&profile,err,sizeof(err)));
    const uint32_t profile_bindings[]={1,2,3,8,10,28,29,32,31};
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,
        "build/diagnostics/fluid_transport_profile.comp.spv",profile_bindings,9,mode));
    VkDescriptorBufferInfo buffers[9];VkWriteDescriptorSet writes[9];
    for(unsigned i=0;i<9;i++) {
        buffers[i]=(VkDescriptorBufferInfo){i<8?f->buffers[i]:profile_buffer,0,i<8?sizes[i]*4:PROFILE_WORDS*4};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
            .dstBinding=profile_bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[i]};
    }
    vkUpdateDescriptorSets(f->p.device,9,writes,0,NULL);
    VkQueryPool pool;VkQueryPoolCreateInfo query={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType=VK_QUERY_TYPE_TIMESTAMP,.queryCount=2};
    ASSERT_EQ(vkCreateQueryPool(f->p.device,&query,NULL,&pool),VK_SUCCESS);
    ASSERT_TRUE(begin(&f->p));vkCmdResetQueryPool(f->p.command,pool,0,2);
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[]={WIDTH,HEIGHT,mode,0,leaves,bits(6.f),1,0};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdWriteTimestamp2(f->p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,0);
    vkCmdDispatch(f->p.command,HEIGHT,1,1);
    vkCmdWriteTimestamp2(f->p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,1);
    host_barrier(&f->p,1);ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&f->p,"partial-recipient work",1));
    uint64_t times[2],after=0;unsigned mismatches=0;
    ASSERT_EQ(vkGetQueryPoolResults(f->p.device,pool,0,2,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT),VK_SUCCESS);
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        uint32_t water=f->words[0][2*atlas(f,(int)x,(int)y)+1];after+=water;
        mismatches+=water!=f->expected[y*WIDTH+x];
    }
    uint32_t counts[PROFILE_WORDS];memcpy(counts,profile,sizeof(counts));
    printf("mode%u requests%u blocker checks%u sources%u team%u GPU%.3fms mismatches%u\n",
        mode,counts[0],counts[1],counts[2],counts[3],(times[1]-times[0])*properties.limits.timestampPeriod/1e6,mismatches);
#ifdef DC_COLOR_CACHE_PROFILE
    uint64_t color_hash=1469598103934665603ull;
    for(unsigned i=0;i<sizes[7];i++) {color_hash^=f->words[7][i];color_hash*=1099511628211ull;}
    printf("mode%u pigment hash%016llx global loads%u stores%u buffer barriers%u\n",mode,
        (unsigned long long)color_hash,counts[4],counts[5],counts[6]);
#endif
    vkDestroyQueryPool(f->p.device,pool,NULL);fixture_release_pipeline(&f->p);
    vkUnmapMemory(f->p.device,profile_memory);vkDestroyBuffer(f->p.device,profile_buffer,NULL);vkFreeMemory(f->p.device,profile_memory,NULL);
    for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_EQ(mismatches,0u);ASSERT_EQ(after,before);
    ASSERT_TRUE(counts[2]>0u&&counts[3]>0u&&counts[3]<=32u);
    ASSERT_TRUE(counts[0]<=counts[2]*MAX_REQUESTS_PER_SOURCE&&counts[1]<=2u*CELLS);
#ifdef DC_COLOR_CACHE_PROFILE
    if(mode==2u) ASSERT_EQ(color_hash,0x028933e7c8fc0d8eull);
    ASSERT_EQ(counts[6],0u);
    ASSERT_TRUE(counts[4]<=CELLS&&counts[5]<=CELLS);
#endif
    PASS();
}
static void test_rightward_partial_recipients_have_bounded_work(void) {partial_case(2);}
static void test_leftward_partial_recipients_have_bounded_work(void) {partial_case(3);}
int main(void) {
    RUN(test_rightward_partial_recipients_have_bounded_work);
    RUN(test_leftward_partial_recipients_have_bounded_work);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
