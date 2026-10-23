#include "body_probe_fixture.h"
static void test_address_directory_grows_without_relocating_prior_body_records(void) {
    probe_t probe={0};ASSERT_TRUE(initialize(&probe));ASSERT_TRUE(allocate_buffer(&probe));
    dc_gpu_t gpu={.device=probe.device,.physical=probe.physical};char err[256];
    dc_gpu_address_buffer_t pages[4]={{0}},directory={0};
    ASSERT_TRUE(create_pipeline(&probe,"build/diagnostics/body_address_probe.spv"));
    const unsigned counts[]={90,200};unsigned allocated=0;
    for(unsigned stage=0;stage<2;stage++) {
        unsigned page_count=(counts[stage]+63u)/64u;
        dc_gpu_address_buffer_t next={0};
        ASSERT_TRUE(dc_gpu_address_buffer_create(&gpu,page_count*sizeof(dc_body_storage_page_t),true,&next,err,sizeof(err)));
        for(unsigned page=0;page<page_count;page++) {
            if(page>=allocated) {
                ASSERT_TRUE(dc_gpu_address_buffer_create(&gpu,64u*sizeof(dc_gpu_body_record_t),true,&pages[page],err,sizeof(err)));
                dc_gpu_body_record_t *records=pages[page].mapped;
                for(unsigned i=0;i<64;i++) records[i].body.width=19u+page;
            }
            dc_body_storage_page_t *entry=(dc_body_storage_page_t *)next.mapped+page;
            entry->records=address_words(pages[page].address);
            entry->generation=100u+page;entry->free_mask[0]=entry->free_mask[1]=0xffffffffu;
        }
        dc_body_storage_root_t *root=(dc_body_storage_root_t *)probe.mapped;
        *root=(dc_body_storage_root_t){.directory=address_words(next.address),.live_count=counts[stage],
            .slab_shift=DC_BODY_SLAB_SHIFT,.abi_version=DC_BODY_STORAGE_ABI,.generation=7u,
            .slab_count=page_count,.directory_capacity=page_count};
        ASSERT_TRUE(begin(&probe));
        VkMemoryBarrier upload={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
        vkCmdPipelineBarrier(probe.command,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&upload,0,NULL,0,NULL);
        vkCmdBindPipeline(probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,probe.pipeline);
        vkCmdBindDescriptorSets(probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,probe.layout,0,1,&probe.descriptor,0,NULL);
        vkCmdDispatch(probe.command,(counts[stage]+63u)/64u,1,1);host_barrier(&probe,1);
        ASSERT_EQ(vkEndCommandBuffer(probe.command),VK_SUCCESS);
        if(!submit(&probe,"addressed body directory",1)) exit(2);
        unsigned mismatches=0;
        for(unsigned body=0;body<counts[stage];body++) {
            dc_gpu_body_record_t *record=(dc_gpu_body_record_t *)pages[body/64].mapped+body%64;
            unsigned visits=stage==1&&body<counts[0]?2u:1u;
            mismatches+=record->body.id!=0x80000001u+body||record->body.width!=19u+body/64||
                record->body.vx_fp!=(int)visits||record->mass!=(float)body+.5f||
                record->motion_flags!=((100u+body/64)^root->generation);
        }
        printf("Addressed body layout%zu bytes, dynamic records%u pages%u mismatches%u\n",sizeof(dc_gpu_body_record_t),counts[stage],page_count,mismatches);
        dc_gpu_address_buffer_destroy(&gpu,&directory);directory=next;allocated=page_count;
        ASSERT_EQ(mismatches,0u);
    }
    dc_gpu_address_buffer_destroy(&gpu,&directory);
    for(unsigned page=0;page<allocated;page++) dc_gpu_address_buffer_destroy(&gpu,&pages[page]);
    ASSERT_EQ(gpu.mapped_local_bytes+gpu.mapped_system_bytes,0u);
    destroy_probe(&probe);PASS();
}
int main(void) {
    RUN(test_address_directory_grows_without_relocating_prior_body_records);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
