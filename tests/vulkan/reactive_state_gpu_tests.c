#define DC_PROBE_WORDS 8192
#define main reactive_state_probe_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "dungeoncraft/reactive_state_layout.h"
#include "ferrum/fire/reactive_state.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static bool dispatch_state(probe_t *p) {
    if(!begin(p)) return false;
    VkMemoryBarrier m={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p->command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&m,0,NULL,0,NULL);
    vkCmdBindPipeline(p->command,VK_PIPELINE_BIND_POINT_COMPUTE,p->pipeline);
    vkCmdBindDescriptorSets(p->command,VK_PIPELINE_BIND_POINT_COMPUTE,p->layout,0,1,&p->descriptor,0,NULL);
    vkCmdDispatch(p->command,1,1,1);host_barrier(p,1);
    return check(vkEndCommandBuffer(p->command),"state command end")&&submit(p,"reactive ownership",1);
}
static void cleanup_state(probe_t *p) {
    vkDestroyPipeline(p->device,p->pipeline,NULL);vkDestroyShaderModule(p->device,p->shader,NULL);
    vkDestroyPipelineLayout(p->device,p->layout,NULL);vkDestroyDescriptorPool(p->device,p->descriptor_pool,NULL);
    vkDestroyDescriptorSetLayout(p->device,p->descriptor_layout,NULL);vkUnmapMemory(p->device,p->memory);
    vkDestroyBuffer(p->device,p->buffer,NULL);vkFreeMemory(p->device,p->memory,NULL);
    vkDestroyFence(p->device,p->fence,NULL);vkDestroyCommandPool(p->device,p->pool,NULL);
    vkDestroyDevice(p->device,NULL);vkDestroyInstance(p->instance,NULL);
}
static void test_gpu_moves_single_owner_preserving_every_state_bit(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p)&&allocate_buffer(&p));
    ASSERT_TRUE(create_pipeline(&p,"build/diagnostics/reactive_state_probe.spv"));
    memset(p.mapped,0,PROBE_WORDS*4u);p.mapped[0]=16;p.mapped[1]=16;
    fr_reactive_state_t state;char err[128];
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,100,700,73,&state,err,sizeof(err)));
    state.ignition_threshold=.7f;state.ignition_integral=.3f;
    state.coating_oil_fuel_fp=300;state.pending_shock_energy=27;
    for(uint32_t i=0;i<16;i++) {
        uint32_t *entry=p.mapped+64+i*FR_ENTRY_WORDS;entry[0]=i+7;entry[1]=FR_REACTIVE_OWNER_CELL;
        memcpy(entry+FR_ENTRY_STATE,&state,sizeof(state));entry[FR_ENTRY_STATE+2]=73+i;
        p.mapped[1024+i*2]=i;p.mapped[1025+i*2]=i+7;
        uint32_t *command=p.mapped+2048+i*10;
        command[0]=1;command[1]=i;command[2]=i+16;command[3]=FR_REACTIVE_OWNER_BODY;
        command[4]=100+i;command[5]=3;command[6]=i*19;command[7]=i*23;
    }
    ASSERT_TRUE(dispatch_state(&p));
    for(uint32_t i=0;i<16;i++) {
        uint32_t *entry=p.mapped+64+i*FR_ENTRY_WORDS,*command=p.mapped+2048+i*10;
        ASSERT_EQ(command[9],1u);ASSERT_EQ(p.mapped[1025+i*2],0u);
        ASSERT_EQ(p.mapped[1024+(i+16)*2],i);ASSERT_EQ(p.mapped[1025+(i+16)*2],i+7);
        ASSERT_EQ(entry[1],FR_REACTIVE_OWNER_BODY);ASSERT_EQ(entry[2],100+i);
        ASSERT_EQ(entry[3],3u);ASSERT_EQ(entry[4],i*19);ASSERT_EQ(entry[5],i*23);
        state.parcel_id_low=73+i;ASSERT_TRUE(memcmp(entry+FR_ENTRY_STATE,&state,sizeof(state))==0);
    }
    cleanup_state(&p);PASS();
}
static void test_stale_or_occupied_handles_never_steal_or_erase_live_state(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p)&&allocate_buffer(&p));
    ASSERT_TRUE(create_pipeline(&p,"build/diagnostics/reactive_state_probe.spv"));
    memset(p.mapped,0,PROBE_WORDS*4u);p.mapped[0]=2;p.mapped[1]=1;
    p.mapped[64]=8;p.mapped[65]=1;p.mapped[100]=3;p.mapped[101]=1;
    p.mapped[1024]=0;p.mapped[1025]=7; /* retired generation */
    p.mapped[1026]=1;p.mapped[1027]=3;
    uint32_t *c=p.mapped+2048;c[0]=1;c[1]=0;c[2]=1;c[3]=2;c[4]=101;c[5]=1;
    uint32_t saved[72];memcpy(saved,p.mapped+64,sizeof(saved));
    ASSERT_TRUE(dispatch_state(&p));ASSERT_EQ(c[9],0u);
    ASSERT_TRUE(memcmp(saved,p.mapped+64,sizeof(saved))==0);ASSERT_EQ(p.mapped[1027],3u);
    p.mapped[1025]=8; /* valid source, occupied destination must also fail */
    ASSERT_TRUE(dispatch_state(&p));ASSERT_EQ(c[9],0u);ASSERT_EQ(p.mapped[1025],8u);
    c[0]=2;ASSERT_TRUE(dispatch_state(&p));ASSERT_EQ(c[9],1u);
    ASSERT_EQ(p.mapped[64],9u);ASSERT_EQ(p.mapped[65],0u);ASSERT_EQ(p.mapped[1025],0u);
    /* Reused slot generation 9 cannot be destroyed through old generation 8. */
    p.mapped[65]=1;p.mapped[1025]=8;
    ASSERT_TRUE(dispatch_state(&p));ASSERT_EQ(c[9],0u);ASSERT_EQ(p.mapped[65],1u);
    cleanup_state(&p);PASS();
}
int main(void) {
    RUN(test_gpu_moves_single_owner_preserving_every_state_bit);
    RUN(test_stale_or_occupied_handles_never_steal_or_erase_live_state);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
