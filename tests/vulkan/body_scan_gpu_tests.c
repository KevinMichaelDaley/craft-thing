#define DC_PROBE_PUSH_CONSTANT_BYTES 24
#include "body_probe_fixture.h"
#include "../../src/vulkan/body_scan_gpu.h"
static void test_parallel_pair_scan_handles_partial_and_recursive_levels_exactly(void) {
    probe_t probe={0};ASSERT_TRUE(initialize(&probe));ASSERT_TRUE(allocate_buffer(&probe));
    ASSERT_TRUE(create_pipeline(&probe,"build/diagnostics/body_scan.spv"));
    uint32_t phase=1;VkSpecializationMapEntry entry={.constantID=0,.size=sizeof(phase)};
    VkSpecializationInfo specialization={.mapEntryCount=1,.pMapEntries=&entry,.dataSize=sizeof(phase),.pData=&phase};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=probe.layout,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=probe.shader,.pName="main",.pSpecializationInfo=&specialization}};
    VkPipeline add=VK_NULL_HANDLE;ASSERT_EQ(vkCreateComputePipelines(probe.device,VK_NULL_HANDLE,1,&info,NULL,&add),VK_SUCCESS);
    dc_gpu_t gpu={.device=probe.device,.physical=probe.physical};char err[128];
    const uint32_t counts[]={1,127,128,129,16384,16385};uint32_t mismatches=0;
    for(uint32_t test=0;test<6;test++) {
        dc_gpu_body_scan_plan_t plan={0};dc_gpu_address_buffer_t scratch={0};
        ASSERT_TRUE(dc_gpu_body_scan_plan_create(counts[test],65535,65535,&plan,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_address_buffer_create(&gpu,plan.bytes+16u,true,&scratch,err,sizeof(err)));
        memset(scratch.mapped,0xab,(size_t)scratch.bytes);uint32_t *words=scratch.mapped;
        for(uint32_t i=0;i<counts[test];i++) {words[i*2]=(i*13u+3u)%65u;words[i*2+1]=64u-words[i*2];}
        ASSERT_TRUE(begin(&probe));
        ASSERT_TRUE(dc_gpu_body_scan_record(probe.command,&plan,scratch.address,plan.bytes,probe.layout,probe.pipeline,add));
        host_barrier(&probe,1);ASSERT_EQ(vkEndCommandBuffer(probe.command),VK_SUCCESS);
        if(!submit(&probe,"hierarchical body free/live scan",1)) exit(2);
        uint32_t free_total=0,owned_total=0,wrong=0;
        for(uint32_t i=0;i<counts[test];i++) {
            uint32_t free=(i*13u+3u)%65u;free_total+=free;owned_total+=64u-free;
            wrong+=words[i*2]!=free_total||words[i*2+1]!=owned_total;
        }
        uint32_t *total=(uint32_t *)((char *)scratch.mapped+plan.total_offset);
        wrong+=total[0]!=free_total||total[1]!=owned_total;
        uint32_t *guard=(uint32_t *)((char *)scratch.mapped+plan.bytes);
        for(uint32_t i=0;i<4;i++) wrong+=guard[i]!=0xababababu;
        printf("body scan count%u levels%u mismatches%u\n",counts[test],plan.level_count,wrong);mismatches+=wrong;
        dc_gpu_address_buffer_destroy(&gpu,&scratch);dc_gpu_body_scan_plan_destroy(&plan);
    }
    vkDestroyPipeline(probe.device,add,NULL);destroy_probe(&probe);
    ASSERT_EQ(gpu.mapped_local_bytes+gpu.mapped_system_bytes,0u);ASSERT_EQ(mismatches,0u);PASS();
}
int main(void) {RUN(test_parallel_pair_scan_handles_partial_and_recursive_levels_exactly);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
