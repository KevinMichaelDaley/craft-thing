#define DC_PROBE_PUSH_CONSTANT_BYTES 48
#include "body_probe_fixture.h"
static uint64_t address_value(dc_body_address_t address) {return (uint64_t)address.low|(uint64_t)address.high<<32;}
static void test_parallel_directory_addresses_preserve_carries_and_large_plane_offsets(void) {
    probe_t probe={0};ASSERT_TRUE(initialize(&probe));ASSERT_TRUE(allocate_buffer(&probe));
    ASSERT_TRUE(create_pipeline(&probe,"build/diagnostics/body_storage_init.spv"));
    dc_gpu_t gpu={.device=probe.device,.physical=probe.physical};char err[128];dc_gpu_address_buffer_t directory={0};
    ASSERT_TRUE(dc_gpu_address_buffer_create(&gpu,64u*sizeof(dc_body_storage_page_t)+16u,true,&directory,err,sizeof(err)));
    memset(directory.mapped,0xab,(size_t)directory.bytes);
    const uint64_t base=0x12345678fffffff0ull;
    const uint32_t added=200000,strides[]={sizeof(dc_gpu_body_record_t),128,sizeof(dc_body_storage_owner_t),32};
    uint32_t constants[]={(uint32_t)directory.address,(uint32_t)(directory.address>>32),
        (uint32_t)base,(uint32_t)(base>>32),0,added,0,0,strides[0],strides[1],strides[2],strides[3]};
    ASSERT_TRUE(begin(&probe));
    VkMemoryBarrier upload={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(probe.command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&upload,0,NULL,0,NULL);
    vkCmdBindPipeline(probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,probe.pipeline);
    vkCmdPushConstants(probe.command,probe.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants);
    vkCmdDispatch(probe.command,1,1,1);host_barrier(&probe,1);
    ASSERT_EQ(vkEndCommandBuffer(probe.command),VK_SUCCESS);
    if(!submit(&probe,"parallel page address arithmetic",1)) exit(2);
    uint64_t planes[]={base,base+(uint64_t)added*64u*strides[0],0,0};
    planes[2]=planes[1]+(uint64_t)added*64u*strides[1];planes[3]=planes[2]+(uint64_t)added*64u*strides[2];
    dc_body_storage_page_t *pages=directory.mapped;uint32_t mismatches=0;
    for(uint32_t page=0;page<64;page++) {
        dc_body_address_t actual[]={pages[page].records,pages[page].solver,pages[page].ownership,pages[page].auxiliary};
        for(uint32_t plane=0;plane<4;plane++) mismatches+=address_value(actual[plane])!=planes[plane]+(uint64_t)page*64u*strides[plane];
        mismatches+=pages[page].free_mask[0]!=UINT32_MAX||pages[page].free_mask[1]!=UINT32_MAX||pages[page].generation!=1u||pages[page].flags!=0u;
    }
    uint32_t *guard=(uint32_t *)(pages+64);
    for(uint32_t i=0;i<4;i++) mismatches+=guard[i]!=0xababababu;
    printf("64 descriptors with virtual multi-gigabyte plane offsets: %u mismatches; addresses never dereferenced\n",mismatches);
    dc_gpu_address_buffer_destroy(&gpu,&directory);destroy_probe(&probe);ASSERT_EQ(mismatches,0u);PASS();
}
int main(void) {RUN(test_parallel_directory_addresses_preserve_carries_and_large_plane_offsets);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
