#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static uint64_t command_hash,secondary_hash;
static uint32_t dispatches,executed,allocated;
static uint64_t program_hashes[32],last_executed_hash;
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
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) {(void)c;(void)p;hash_word(1);hash_word((uintptr_t)pipeline);}
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags f,uint32_t o,uint32_t n,const void *v) {
    (void)c;(void)l;(void)f;hash_word(2);hash_word(o);hash_word(n);
    for(unsigned i=0;i<n;i++) hash_word(((const uint8_t *)v)[i]);
}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;hash_word(3);hash_word(x);hash_word(y);hash_word(z);dispatches++;}
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer c,VkBuffer b,VkDeviceSize o) {(void)c;hash_word(4);hash_word((uintptr_t)b);hash_word(o);dispatches++;}
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
int main(void) {
    RUN(test_reuses_the_exact_solver_program_without_recording_again);
    RUN(test_rebuilds_for_origin_growth_descriptor_and_structural_changes);
    RUN(test_reuses_exact_mpm_commands_without_repeating_cpu_recording);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
