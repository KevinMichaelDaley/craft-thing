#define DC_PROBE_WORDS (16+4*1025)
#include "body_probe_fixture.h"
#include "../../src/vulkan/body_storage_gpu.h"
static void test_device_local_store_growth_copies_directory_preserves_records_and_live_count(void) {
    probe_t probe={0};ASSERT_TRUE(initialize(&probe));ASSERT_TRUE(allocate_buffer(&probe));
    ASSERT_TRUE(create_pipeline(&probe,"build/diagnostics/body_growth_probe.spv"));
    dc_gpu_t gpu={.device=probe.device,.physical=probe.physical};
    dc_gpu_body_store_t store={0};dc_gpu_body_growth_t growth={0};char err[256];
    const uint32_t counts[]={90,200,1025};
    memset(probe.mapped,0,PROBE_WORDS*sizeof(uint32_t));
    for(uint32_t stage=0;stage<3;stage++) {
        ASSERT_TRUE(dc_gpu_body_growth_prepare(&gpu,&store,counts[stage],&growth,err,sizeof(err)));
        ASSERT_TRUE(begin(&probe));
        ASSERT_TRUE(dc_gpu_body_growth_record(probe.command,probe.buffer,&store,&growth,stage+1));
        VkMemoryBarrier overwrite={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT};
        vkCmdPipelineBarrier(probe.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&overwrite,0,NULL,0,NULL);
        vkCmdUpdateBuffer(probe.command,probe.buffer,offsetof(dc_body_storage_root_t,live_count),sizeof(uint32_t),&counts[stage]);
        VkMemoryBarrier upload={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(probe.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&upload,0,NULL,0,NULL);
        vkCmdBindPipeline(probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,probe.pipeline);
        vkCmdBindDescriptorSets(probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,probe.layout,0,1,&probe.descriptor,0,NULL);
        vkCmdDispatch(probe.command,(counts[stage]+63u)/64u,1,1);
        VkMemoryBarrier readback={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT};
        vkCmdPipelineBarrier(probe.command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&readback,0,NULL,0,NULL);
        ASSERT_EQ(vkEndCommandBuffer(probe.command),VK_SUCCESS);
        if(!submit(&probe,"transactional body store growth",1)) exit(2);
        ASSERT_TRUE(dc_gpu_body_growth_finish(&gpu,&store,&growth,stage+1));
        dc_body_storage_root_t *root=(dc_body_storage_root_t *)probe.mapped;
        uint32_t *results=(uint32_t *)(root+1);unsigned mismatches=0;
        for(uint32_t body=0;body<counts[stage];body++) {
            uint32_t visits=1;
            for(uint32_t previous=0;previous<stage;previous++) visits+=body<counts[previous];
            float mass=(float)body+.5f;uint32_t mass_bits;memcpy(&mass_bits,&mass,sizeof(mass_bits));
            mismatches+=results[body*4]!=0x80000001u+body||results[body*4+1]!=visits||
                results[body*4+2]!=mass_bits||results[body*4+3]!=1u;
        }
        printf("GPU-owned body growth records%u slabs%u mismatches%u\n",counts[stage],store.slab_count,mismatches);
        ASSERT_EQ(mismatches,0u);ASSERT_EQ(root->live_count,counts[stage]);
    }
    dc_gpu_body_store_destroy(&gpu,&store);
    ASSERT_EQ(gpu.device_only_bytes+gpu.mapped_local_bytes+gpu.mapped_system_bytes,0u);
    destroy_probe(&probe);PASS();
}
int main(void) {
    RUN(test_device_local_store_growth_copies_directory_preserves_records_and_live_count);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
