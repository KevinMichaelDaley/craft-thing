#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/runtime_body_internal.h"

static int g_pass,g_fail;
static uint64_t command_hash,secondary_hash;
static uint32_t dispatches,executed,allocated;
static uint64_t program_hashes[256],last_executed_hash;
static dc_gpu_t *expected_mpm;
static VkPipeline bound;
static uint32_t pushed_mode,mpm_modes,bad_mpm_bindings,generic_mpm_binds;
static const uint64_t HASH_SEED=1469598103934665603ull;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void hash_word(uint64_t value) {command_hash=(command_hash^value)*1099511628211ull;}
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint p,VkPipelineLayout l,uint32_t first,uint32_t count,const VkDescriptorSet *s,uint32_t dynamic_count,const uint32_t *offsets) {
    (void)c;(void)p;(void)l;(void)first;(void)count;(void)s;(void)dynamic_count;(void)offsets;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) {(void)c;(void)p;bound=pipeline;
    if(expected_mpm&&pipeline==expected_mpm->mpm_pipeline) generic_mpm_binds++;
    hash_word(1);hash_word((uintptr_t)pipeline);}
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags f,uint32_t o,uint32_t n,const void *v) {
    (void)c;(void)l;(void)f;hash_word(2);hash_word(o);hash_word(n);
    if(o==0u&&n>=12u) memcpy(&pushed_mode,(const uint8_t *)v+8u,4u);
    for(unsigned i=0;i<n;i++) hash_word(((const uint8_t *)v)[i]);
}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;hash_word(3);hash_word(x);hash_word(y);hash_word(z);dispatches++;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer c,VkBuffer b,VkDeviceSize o) {(void)c;hash_word(4);hash_word((uintptr_t)b);hash_word(o);dispatches++;
    if(expected_mpm&&pushed_mode<8u&&(bound==expected_mpm->mpm_pipeline||((uintptr_t)bound>=0x200u&&(uintptr_t)bound<0x208u))) {
        mpm_modes|=1u<<pushed_mode;bad_mpm_bindings+=bound!=expected_mpm->mpm_phase_pipelines[pushed_mode];
    }
}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *info) {
    (void)c;hash_word(5);hash_word(info->memoryBarrierCount);
    for(unsigned i=0;i<info->memoryBarrierCount;i++) {
        const VkMemoryBarrier2 *m=info->pMemoryBarriers+i;
        hash_word(m->srcStageMask);hash_word(m->srcAccessMask);hash_word(m->dstStageMask);hash_word(m->dstAccessMask);
    }
}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice d,const VkCommandBufferAllocateInfo *i,VkCommandBuffer *out) {
    (void)d;if(i->level!=VK_COMMAND_BUFFER_LEVEL_SECONDARY||i->commandBufferCount!=1u) return VK_ERROR_INITIALIZATION_FAILED;
    *out=(VkCommandBuffer)(uintptr_t)(10u+allocated++);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice d,VkCommandPool p,uint32_t n,const VkCommandBuffer *b) {(void)d;(void)p;(void)n;(void)b;}
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer c,VkCommandBufferResetFlags f) {(void)c;(void)f;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo *i) {
    (void)c;if(!i->pInheritanceInfo) return VK_ERROR_INITIALIZATION_FAILED;
    command_hash=HASH_SEED;return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer c) {secondary_hash=command_hash;program_hashes[(uintptr_t)c-10u]=command_hash;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer c,uint32_t n,const VkCommandBuffer *b) {(void)c;for(unsigned i=0;i<n;i++) {last_executed_hash=program_hashes[(uintptr_t)b[i]-10u];executed++;}}
static dc_gpu_t fake_gpu(void) {
    dc_gpu_t g={.width=64,.height=64,.page_width=1,.page_height=1,.body_count=1,
        .device=(VkDevice)(uintptr_t)8,.command_pool=(VkCommandPool)(uintptr_t)7,
        .command=(VkCommandBuffer)(uintptr_t)9,.descriptor=(VkDescriptorSet)(uintptr_t)5,
        .pipeline_layout=(VkPipelineLayout)(uintptr_t)6,.body_buffer=(VkBuffer)(uintptr_t)3,
        .mpm_activity_buffer=(VkBuffer)(uintptr_t)4};
    for(unsigned i=0;i<DC_GPU_SOLVER_PHASE_COUNT;i++) g.solver_pipelines[i]=(VkPipeline)(uintptr_t)(i+100);
    return g;
}
static void test_reuses_the_exact_solver_program_without_recording_again(void) {
    dc_gpu_t g=fake_gpu();char err[128];command_hash=HASH_SEED;dispatches=executed=0;
    dc_gpu_record_solver(&g);uint64_t reference=command_hash;uint32_t reference_dispatches=dispatches;
    ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
    dispatches=0;dc_gpu_record_solver(&g);
    ASSERT_EQ(secondary_hash,reference);ASSERT_EQ(dispatches,reference_dispatches);
    dc_gpu_record_solver(&g);ASSERT_EQ(dispatches,reference_dispatches);
    uint64_t recordings,executions;ASSERT_TRUE(dc_gpu_solver_commands_stats(&g,&recordings,&executions));
    ASSERT_EQ(recordings,1u);ASSERT_EQ(executions,2u);ASSERT_EQ(executed,2u);
    ASSERT_EQ(g.command,(VkCommandBuffer)(uintptr_t)9);
    dc_gpu_solver_commands_destroy(&g);ASSERT_TRUE(!g.solver_commands);PASS();
}
static void test_rebuilds_for_origin_growth_descriptor_and_structural_changes(void) {
    dc_gpu_t g=fake_gpu();char err[128];ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
    dc_gpu_record_solver(&g);
    g.body_origin.x++;dc_gpu_record_solver(&g);
    g.body_count++;dc_gpu_record_solver(&g);
    g.descriptor_revision++;dc_gpu_record_solver(&g);
    g.body_buffer=(VkBuffer)(uintptr_t)33;dc_gpu_record_solver(&g);
    g.structural_enabled=true;dc_gpu_record_solver(&g);
    uint64_t recordings,executions;ASSERT_TRUE(dc_gpu_solver_commands_stats(&g,&recordings,&executions));
    ASSERT_EQ(recordings,6u);ASSERT_EQ(executions,6u);
    g.solver_cache_disabled=true;dc_gpu_record_solver(&g);
    ASSERT_TRUE(dc_gpu_solver_commands_stats(&g,&recordings,&executions));
    ASSERT_EQ(recordings,6u);ASSERT_EQ(executions,6u);
    dc_gpu_solver_commands_destroy(&g);PASS();
}
static void test_rebuilds_when_independent_apply_overlap_changes(void) {
    uint32_t header[64]={0};header[9]=256;header[10]=512;
    struct dc_runtime_bodies runtime={.work={.mapped=header},.store={.slab_count=1}};
    dc_gpu_t g=fake_gpu();char err[128];
    g.runtime_body_consumers=true;g.runtime_bodies=&runtime;
    ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
    dc_gpu_record_solver(&g);uint64_t conservative=last_executed_hash;
    g.solver_apply_overlap=true;dc_gpu_record_solver(&g);
    uint64_t recordings,executions;ASSERT_TRUE(dc_gpu_solver_commands_stats(&g,&recordings,&executions));
    ASSERT_EQ(recordings,2u);ASSERT_EQ(executions,2u);
    ASSERT_TRUE(last_executed_hash!=conservative);
    dc_gpu_solver_commands_destroy(&g);PASS();
}
static void test_reuses_exact_mpm_commands_without_repeating_cpu_recording(void) {
    dc_gpu_t g=fake_gpu();char err[128];
    ASSERT_TRUE(dc_gpu_mpm_scan_layout(g.width,g.height,&g.mpm_scan_layout));
    command_hash=HASH_SEED;dispatches=0;dc_gpu_record_mpm(&g);
    uint64_t reference=command_hash;uint32_t reference_dispatches=dispatches;
    ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
    command_hash=HASH_SEED;dispatches=0;dc_gpu_record_mpm(&g);
    bool exact=command_hash==reference;
    dc_gpu_record_mpm(&g);bool reused=dispatches==reference_dispatches;
    bool executed_exact=last_executed_hash==reference;
    dc_gpu_solver_commands_destroy(&g);
    printf("MPM exact stream%d, reused%d, executed exact%d\n",exact,reused,executed_exact);
    ASSERT_TRUE(exact);ASSERT_TRUE(reused);ASSERT_TRUE(executed_exact);PASS();
}
static void test_mpm_uses_a_separate_pipeline_for_each_unchanged_phase(void) {
    dc_gpu_t g=fake_gpu();ASSERT_TRUE(dc_gpu_mpm_scan_layout(g.width,g.height,&g.mpm_scan_layout));
    g.mpm_pipeline=(VkPipeline)(uintptr_t)1000u;
    for(unsigned mode=0;mode<8;mode++) g.mpm_phase_pipelines[mode]=(VkPipeline)(uintptr_t)(0x200u+mode);
    expected_mpm=&g;mpm_modes=bad_mpm_bindings=0u;dc_gpu_record_mpm(&g);expected_mpm=NULL;
    printf("MPM phase mask0x%x incorrect pipeline bindings%u\n",mpm_modes,bad_mpm_bindings);
    ASSERT_EQ(mpm_modes,255u);ASSERT_EQ(bad_mpm_bindings,0u);PASS();
}
static void test_generic_mpm_binds_once_while_executing_all_eight_phases(void) {
    dc_gpu_t g=fake_gpu();ASSERT_TRUE(dc_gpu_mpm_scan_layout(g.width,g.height,&g.mpm_scan_layout));
    g.mpm_pipeline=(VkPipeline)(uintptr_t)1000u;
    expected_mpm=&g;mpm_modes=generic_mpm_binds=0u;dc_gpu_record_mpm(&g);expected_mpm=NULL;
    printf("generic MPM phase mask0x%x pipeline binds%u\n",mpm_modes,generic_mpm_binds);
    ASSERT_EQ(mpm_modes,255u);ASSERT_EQ(generic_mpm_binds,1u);PASS();
}

static void test_reuses_exact_fluid_phases_and_invalidates_changed_parameters(void) {
    for(unsigned phase=0;phase<5;phase++) {
        dc_gpu_t g=fake_gpu();char err[128];
        g.fluid_step_scale=2.0f;g.timed_fluid=true;g.rigid_occupancy_present=true;
        for(unsigned parity=0;parity<2;parity++) {
            g.fluid_tick=parity;command_hash=HASH_SEED;dispatches=0;
            g.solver_cache_disabled=true;dc_gpu_record_fluid_phase(&g,phase);g.solver_cache_disabled=false;
            uint64_t reference=command_hash;uint32_t count=dispatches;
            if(!g.solver_commands) ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
            dispatches=0;dc_gpu_record_fluid_phase(&g,phase);
            bool exact=last_executed_hash==reference;
            dc_gpu_record_fluid_phase(&g,phase);
            bool reused=dispatches==count;
            printf("fluid phase%u parity%u exact%d reused%d\n",phase,parity,exact,reused);
            ASSERT_TRUE(exact);ASSERT_TRUE(reused);
            g.fluid_snapshot_valid=false;dc_gpu_record_fluid_phase(&g,phase);
            if(phase==3u) ASSERT_TRUE(g.fluid_snapshot_valid);
            g.fluid_step_scale=1.0f;dispatches=0;dc_gpu_record_fluid_phase(&g,phase);
            ASSERT_TRUE(dispatches>0u);g.fluid_step_scale=2.0f;
        }
        dc_gpu_solver_commands_destroy(&g);
    }
    PASS();
}
static void test_graph_authoring_invalidates_cached_empty_graph_commands(void) {
    dc_gpu_t g=fake_gpu();char err[128];g.structural_enabled=true;
    ASSERT_TRUE(dc_gpu_solver_commands_init(&g,err,sizeof(err)));
    dc_gpu_record_solver(&g);uint64_t empty=last_executed_hash;
    g.structural_graph_possible=true;dc_gpu_record_solver(&g);
    uint64_t recordings,executions;ASSERT_TRUE(dc_gpu_solver_commands_stats(&g,&recordings,&executions));
    ASSERT_EQ(recordings,2u);ASSERT_TRUE(last_executed_hash!=empty);
    dc_gpu_solver_commands_destroy(&g);PASS();
}
int main(void) {
    RUN(test_graph_authoring_invalidates_cached_empty_graph_commands);
    RUN(test_reuses_the_exact_solver_program_without_recording_again);
    RUN(test_rebuilds_for_origin_growth_descriptor_and_structural_changes);
    RUN(test_rebuilds_when_independent_apply_overlap_changes);
    RUN(test_reuses_exact_mpm_commands_without_repeating_cpu_recording);
    RUN(test_mpm_uses_a_separate_pipeline_for_each_unchanged_phase);
    RUN(test_generic_mpm_binds_once_while_executing_all_eight_phases);
    RUN(test_reuses_exact_fluid_phases_and_invalidates_changed_parameters);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
