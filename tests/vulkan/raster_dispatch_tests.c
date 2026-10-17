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
static void selector_case(unsigned kind) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical,.width=64,.height=64,.page_width=1,.page_height=1};
    VkBuffer body,extra[3];VkDeviceMemory body_memory,extra_memory[3];uint32_t *extra_words[3];
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,dc_gpu_body_storage_bytes(&gpu),&body,&body_memory,&gpu.body_mapped,err,sizeof(err)));
    memset(gpu.body_mapped,0,dc_gpu_body_storage_bytes(&gpu));
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,39u*4,&p.buffer,&p.memory,(void **)&p.mapped,err,sizeof(err)));
    const uint32_t sizes[]={4096u*2u,1u,4096u*5u};
    for(unsigned i=0;i<3;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,sizes[i]*4,&extra[i],&extra_memory[i],(void **)&extra_words[i],err,sizeof(err)));
        memset(extra_words[i],0,sizes[i]*4);
    }
    extra_words[1][0]=1;
    dc_gpu_body_record_t *records=gpu.body_mapped;
    records[0].body=(dc_gpu_body_t){.id=37,.active=1,.width=1,.height=1,.x_fp=10<<16,.y_fp=10<<16};
    records[0].visible=1;records[0].motion_ready=1;records[0].mass=.6f;records[0].inertia=.1f;records[0].density=.6f;
    records[0].center[0]=records[0].center[1]=.5f;
    records[0].local_x_fp=records[0].local_y_fp=10<<16;
    records[0].piece_count=DC_GPU_PIXEL_BODY_FLAG|1u;
    records[0].compound_reserved=dc_gpu_pixel_pool_word_offset(&gpu)+DC_GPU_PIXEL_POOL_HEADER_WORDS;
    uint32_t *shape=(uint32_t *)gpu.body_mapped+records[0].compound_reserved;
    const uint32_t header[]={42,1,1,1,0,0,1,1,9,0,2};memcpy(shape,header,sizeof(header));
    records[1]=records[0];records[1].body.id=38;records[1].piece_count=kind==0?DC_GPU_PIXEL_BODY_FLAG|1u:0u;
    records[1].body.active=kind==0||kind==2||kind==4||kind==7;
    records[1].visible=kind!=4&&kind!=7;
    if(kind==1) records[0].piece_count=0;
    dc_gpu_structural_data(&gpu)[16]=kind==6?4:0;
    for(unsigned i=0;i<39;i++) p.mapped[i]=4369;
    const uint32_t bindings[]={1,2,3,4,27};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,body,"build/shaders/rigid.comp.spv",bindings,5,2));
    VkDescriptorBufferInfo buffers[3];VkWriteDescriptorSet writes[3];
    for(unsigned i=0;i<3;i++) {
        buffers[i]=(VkDescriptorBufferInfo){extra[i],0,sizes[i]*4};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,
            .dstBinding=i+1,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[i]};
    }
    vkUpdateDescriptorSets(p.device,3,writes,0,NULL);ASSERT_TRUE(begin(&p));
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    uint32_t push[8]={64,64,kind==7?512u:kind>=5?256u:0u,2,64,64};
    vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    /* Poisoned triplets are inspected after one bounded direct group, never executed. */
    vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&p,"raster selector",1));
    bool generic=kind==1||kind==2||kind==7,refresh=kind!=5;
    const uint32_t expected[]={generic&&refresh?4u:0u,generic&&refresh?4u:1u,1,
        !generic&&refresh?4u:0u,!generic&&refresh?4u:1u,1};
    bool safe=true;
    for(unsigned i=0;i<6;i++) {
        if(p.mapped[i]!=expected[i]) printf("kind%u word%u: %u expected%u\n",kind,i,p.mapped[i],expected[i]);
        safe=safe&&p.mapped[i]==expected[i];
    }
    for(unsigned i=6;i<39;i++) safe=safe&&p.mapped[i]==4369u;
    fixture_release_pipeline(&p);
    for(unsigned i=0;i<3;i++) {vkUnmapMemory(p.device,extra_memory[i]);vkDestroyBuffer(p.device,extra[i],NULL);vkFreeMemory(p.device,extra_memory[i],NULL);}
    vkUnmapMemory(p.device,body_memory);vkDestroyBuffer(p.device,body,NULL);vkFreeMemory(p.device,body_memory,NULL);
    vkUnmapMemory(p.device,p.memory);vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(safe);PASS();
}
static void test_pixel_only_dispatch_resets_both_triplets(void) {selector_case(0);}
static void test_legacy_dispatch_resets_unused_pixel_triplet(void) {selector_case(1);}
static void test_mixed_bodies_use_exact_generic_raster(void) {selector_case(2);}
static void test_disabled_legacy_does_not_disable_pixel_path(void) {selector_case(3);}
static void test_invisible_legacy_does_not_disable_pixel_path(void) {selector_case(4);}
static void test_no_refresh_resets_both_triplets_to_zero_work(void) {selector_case(5);}
static void test_refresh_reclassifies_current_bodies(void) {selector_case(6);}
static void test_draw_selector_includes_invisible_enabled_legacy(void) {selector_case(7);}
int main(void) {
    RUN(test_pixel_only_dispatch_resets_both_triplets);RUN(test_legacy_dispatch_resets_unused_pixel_triplet);
    RUN(test_mixed_bodies_use_exact_generic_raster);RUN(test_disabled_legacy_does_not_disable_pixel_path);
    RUN(test_invisible_legacy_does_not_disable_pixel_path);RUN(test_no_refresh_resets_both_triplets_to_zero_work);
    RUN(test_refresh_reclassifies_current_bodies);RUN(test_draw_selector_includes_invisible_enabled_legacy);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
