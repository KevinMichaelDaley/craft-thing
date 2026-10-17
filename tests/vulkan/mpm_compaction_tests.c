#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();if(g_fail)return 1;} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

enum { SCAN_LANES=256, POISON=4369, GUARD_WORDS=16, SAMPLES=5 };

static uint32_t storage_words(uint32_t tiles) {
    uint32_t words=10u+2u*tiles,count=(tiles+SCAN_LANES-1u)/SCAN_LANES;
    for(;;) {
        words+=count;
        if(count<=SCAN_LANES) {words++;break;}
        count=(count+SCAN_LANES-1u)/SCAN_LANES;
    }
    return words<39u?39u:words;
}
static bool selected(uint32_t tile,unsigned pattern,uint32_t count) {
    return pattern==0?false:pattern==1?tile==count/2u:pattern==2?tile%997u==13u:true;
}
static uint32_t reference(uint32_t *result,uint32_t width,uint32_t height,unsigned pattern) {
    uint32_t used=0,count=width*height;
    for(uint32_t y=0;y<height;y++) for(uint32_t x=0;x<width;x++) {
        bool active=false;
        for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) {
            int nx=(int)x+dx,ny=(int)y+dy;
            if(nx>=0&&ny>=0&&nx<(int)width&&ny<(int)height&&selected((uint32_t)ny*width+(uint32_t)nx,pattern,count)) active=true;
        }
        if(active) result[used++]=y*width+x;
    }
    return used;
}
static int duration_order(const void *a,const void *b) {
    double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);
}
static void compaction_case(uint32_t width,uint32_t height,unsigned pattern,bool clean,bool timed) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical,.width=width,.height=height,
        .page_width=(width+63u)/64u,.page_height=(height+63u)/64u};
    VkBuffer body;VkDeviceMemory body_memory;void *body_mapped;
    VkDeviceSize body_bytes=dc_gpu_body_storage_bytes(&gpu);
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,body_bytes,&body,&body_memory,&body_mapped,err,sizeof(err)));
    memset(body_mapped,0,body_bytes);
    VkBuffer empty;VkDeviceMemory empty_memory;void *empty_mapped;
    uint32_t empty_words=gpu.page_width*gpu.page_height;
    if(empty_words<64u) empty_words=64u;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,empty_words*4u,&empty,&empty_memory,&empty_mapped,err,sizeof(err)));
    memset(empty_mapped,0,empty_words*4u);
    uint32_t tiles_x=(width+15u)/16u,tiles_y=(height+15u)/16u,tiles=tiles_x*tiles_y,words=storage_words(tiles);
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,(words+GUARD_WORDS)*4u,&p.buffer,&p.memory,(void **)&p.mapped,err,sizeof(err)));
    const uint32_t bindings[]={1,2,4,20,27};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,body,"build/shaders/mpm_active.comp.spv",bindings,5,UINT32_MAX));
    VkDescriptorBufferInfo activity={p.buffer,0,words*4u};
    VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,.dstBinding=27,
        .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&activity};
    vkUpdateDescriptorSets(p.device,1,&write,0,NULL);
    VkDescriptorBufferInfo empty_info={empty,0,empty_words*4u};
    const uint32_t empty_bindings[]={1,2,20};
    for(unsigned i=0;i<3;i++) {
        write.dstBinding=empty_bindings[i];write.pBufferInfo=&empty_info;
        vkUpdateDescriptorSets(p.device,1,&write,0,NULL);
    }
    uint32_t *expected=malloc(tiles*4u);ASSERT_TRUE(expected);
    uint32_t count=clean?0:reference(expected,tiles_x,tiles_y,pattern);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(p.physical,&properties);
    VkQueryPool pool;VkQueryPoolCreateInfo query={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType=VK_QUERY_TYPE_TIMESTAMP,.queryCount=2};
    ASSERT_EQ(vkCreateQueryPool(p.device,&query,NULL,&pool),VK_SUCCESS);
    double samples[SAMPLES];unsigned repeats=timed?SAMPLES:1;
    for(unsigned sample=0;sample<repeats;sample++) {
        for(uint32_t word=0;word<words+GUARD_WORDS;word++) p.mapped[word]=POISON;
        for(uint32_t tile=0;tile<tiles;tile++) p.mapped[3u+tiles+tile]=selected(tile,pattern,tiles);
        ASSERT_TRUE(begin(&p));vkCmdResetQueryPool(p.command,pool,0,2);
        VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
            .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memory,0,NULL,0,NULL);
        vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
        uint32_t push[]={width,height,1,clean?1u:0u,0,0,0};
        vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdWriteTimestamp2(p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,0);
        vkCmdDispatch(p.command,1,1,1);
        vkCmdWriteTimestamp2(p.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,pool,1);
        host_barrier(&p,1);ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);
        ASSERT_TRUE(submit(&p,"active tile compaction",1));
        uint64_t times[2];ASSERT_EQ(vkGetQueryPoolResults(p.device,pool,0,2,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT),VK_SUCCESS);
        samples[sample]=(times[1]-times[0])*properties.limits.timestampPeriod/1e6;
        ASSERT_EQ(p.mapped[0],count);ASSERT_EQ(p.mapped[1],1u);ASSERT_EQ(p.mapped[2],1u);
        ASSERT_TRUE(memcmp(p.mapped+3,expected,count*4u)==0);
        uint32_t control=3u+2u*tiles;
        ASSERT_EQ(p.mapped[control],0u);ASSERT_EQ(p.mapped[control+1],count);
        ASSERT_EQ(p.mapped[control+2],1u);ASSERT_EQ(p.mapped[control+3],1u);
        for(uint32_t word=words;word<words+GUARD_WORDS;word++) ASSERT_EQ(p.mapped[word],POISON);
    }
    qsort(samples,repeats,sizeof(*samples),duration_order);double median=samples[repeats/2];
    printf("%ux%u tile metadata: %u selected, pattern%u clean%d median %.3fms\n",tiles_x,tiles_y,count,pattern,clean,median);
    free(expected);vkDestroyQueryPool(p.device,pool,NULL);fixture_release_pipeline(&p);
    vkUnmapMemory(p.device,body_memory);vkDestroyBuffer(p.device,body,NULL);vkFreeMemory(p.device,body_memory,NULL);
    vkUnmapMemory(p.device,empty_memory);vkDestroyBuffer(p.device,empty,NULL);vkFreeMemory(p.device,empty_memory,NULL);
    vkUnmapMemory(p.device,p.memory);vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    double budget=.350+(tiles>3600u?(tiles-3600u)*.000010:0.);
    if(timed) ASSERT_TRUE(median<=budget);
    PASS();
}
static void test_empty_tiles_reset_poisoned_indirect_headers(void) {compaction_case(640,360,0,false,false);}
static void test_sparse_halo_is_sorted_across_partial_scan_blocks(void) {compaction_case(2113,17,2,false,false);}
static void test_dense_partial_blocks_keep_every_tile_once(void) {compaction_case(640,360,3,false,false);}
static void test_clean_rigid_cache_resets_all_work(void) {compaction_case(64,64,3,true,false);}
static void test_prefix_hierarchy_preserves_large_sparse_order(void) {compaction_case(4097,4097,2,false,false);}
static void test_compaction_parallel_work_budget(void) {compaction_case(1280,720,0,false,true);}
static void test_large_compaction_has_parallel_scaling(void) {compaction_case(4097,4097,0,false,true);}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc==2&&strcmp(argv[1],"performance")==0) {
        RUN(test_compaction_parallel_work_budget);
        RUN(test_large_compaction_has_parallel_scaling);
    } else {
        RUN(test_empty_tiles_reset_poisoned_indirect_headers);RUN(test_sparse_halo_is_sorted_across_partial_scan_blocks);
        RUN(test_dense_partial_blocks_keep_every_tile_once);RUN(test_clean_rigid_cache_resets_all_work);
        RUN(test_prefix_hierarchy_preserves_large_sparse_order);
    }
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
