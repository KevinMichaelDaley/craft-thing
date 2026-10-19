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
    VkBuffer body;VkDeviceMemory body_memory;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,dc_gpu_body_storage_bytes(&gpu),&body,&body_memory,&gpu.body_mapped,err,sizeof(err)));
    memset(gpu.body_mapped,0,dc_gpu_body_storage_bytes(&gpu));
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,39u*4,&p.buffer,&p.memory,(void **)&p.mapped,err,sizeof(err)));
    for(unsigned i=0;i<39;i++) p.mapped[i]=4369;
    dc_gpu_body_record_t *records=gpu.body_mapped;
    records[0].body=(dc_gpu_body_t){.id=37,.active=kind!=4,.width=1,.height=1};
    records[0].piece_count=kind==3?0:DC_GPU_PIXEL_BODY_FLAG|1u;
    records[63].body=(dc_gpu_body_t){.id=38,.active=kind==1,.width=1,.height=1};
    uint32_t *pairs=dc_gpu_broadphase_data(&gpu),*contacts=dc_gpu_contact_data(&gpu),*solver=dc_gpu_solver_data(&gpu);
    uint32_t pair_count=kind==8?4096u:kind==9?0u:19u;
    pairs[0]=pair_count;pairs[2]=kind==5?4:0;contacts[3]=777;
    for(unsigned i=10;i<16;i++) solver[i]=4369;
    const uint32_t bindings[]={1,2,4,19,27};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,body,"build/shaders/contacts.comp.spv",bindings,5,0));
    ASSERT_TRUE(begin(&p));
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    uint32_t push[]={64,64,kind==7?0u:1024u|(kind==6?512u:0u),64,0,0,0,0};
    vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    /* Inspect poisoned triplets after one direct group; never execute poison. */
    vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);
    ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);ASSERT_TRUE(submit(&p,"contact selector",1));
    bool pixels=kind!=3&&kind!=4,typed=pixels&&kind!=1&&kind!=5&&kind!=6&&kind!=7;
    uint32_t generic=pixels&&!typed&&kind!=5?pair_count:0u,fast=typed?pair_count:0u;
    bool safe=solver[10]==generic&&solver[11]==1&&solver[12]==1&&
        p.mapped[15]==fast&&p.mapped[16]==(fast?16u:1u)&&p.mapped[17]==1&&contacts[3]==777&&
        p.mapped[18]==fast&&p.mapped[19]==1&&p.mapped[20]==1&&
        p.mapped[21]==(fast+63u)/64u&&p.mapped[22]==1&&p.mapped[23]==1&&
        contacts[6]==(kind==5?0u:(pair_count+63u)/64u);
    /* The final solver triplet now belongs to grain coupling. It must start
       inactive even when the old contents were poisoned. */
    safe=safe&&solver[13]==0u&&solver[14]==1u&&solver[15]==1u;
    for(unsigned i=0;i<39;i++) if(i<15||i>23) safe=safe&&p.mapped[i]==4369;
    printf("kind%u generic %u,%u,%u typed %u,%u,%u\n",kind,solver[10],solver[11],solver[12],p.mapped[15],p.mapped[16],p.mapped[17]);
    fixture_release_pipeline(&p);
    vkUnmapMemory(p.device,body_memory);vkDestroyBuffer(p.device,body,NULL);vkFreeMemory(p.device,body_memory,NULL);
    vkUnmapMemory(p.device,p.memory);vkDestroyBuffer(p.device,p.buffer,NULL);vkFreeMemory(p.device,p.memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(safe);PASS();
}
static void test_pixel_contacts_select_typed_triplet(void) {selector_case(0);}
static void test_hidden_legacy_preserves_generic_contacts(void) {selector_case(1);}
static void test_disabled_legacy_does_not_disable_typed_contacts(void) {selector_case(2);}
static void test_ordinary_only_resets_both_pixel_triplets(void) {selector_case(3);}
static void test_empty_body_set_resets_both_triplets(void) {selector_case(4);}
static void test_broadphase_overflow_disables_both_triplets(void) {selector_case(5);}
static void test_generic_reference_can_be_selected_explicitly(void) {selector_case(6);}
static void test_older_recorders_keep_the_generic_dispatch_contract(void) {selector_case(7);}
static void test_maximum_pair_count_keeps_each_dispatch_axis_bounded(void) {selector_case(8);}
static void test_empty_pair_set_resets_all_planning_triplets(void) {selector_case(9);}
int main(void) {
    RUN(test_pixel_contacts_select_typed_triplet);RUN(test_hidden_legacy_preserves_generic_contacts);
    RUN(test_disabled_legacy_does_not_disable_typed_contacts);RUN(test_ordinary_only_resets_both_pixel_triplets);
    RUN(test_empty_body_set_resets_both_triplets);RUN(test_broadphase_overflow_disables_both_triplets);
    RUN(test_generic_reference_can_be_selected_explicitly);
    RUN(test_older_recorders_keep_the_generic_dispatch_contract);
    RUN(test_maximum_pair_count_keeps_each_dispatch_axis_bounded);
    RUN(test_empty_pair_set_resets_all_planning_triplets);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
