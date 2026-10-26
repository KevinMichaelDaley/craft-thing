#ifndef GAS_FLAGS_CASES_H
#define GAS_FLAGS_CASES_H
static void test_native_repair_flags_preserve_divergent_masks_with_bounded_global_updates(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err)));
    dc_gpu_address_buffer_t model={0},stats={0};fr_renderer_compute_t compute={0};
    ASSERT_TRUE(dc_gpu_address_buffer_create(g,FR_GAS_MODEL_WORDS*4u,true,&model,err,sizeof(err))&&dc_gpu_address_buffer_create(g,64u,true,&stats,err,sizeof(err)));
    VkDescriptorBufferInfo buffers[12];for(uint32_t i=0;i<12u;i++) buffers[i]=(VkDescriptorBufferInfo){model.buffer,0,model.bytes};
    buffers[6]=(VkDescriptorBufferInfo){stats.buffer,0,stats.bytes};
    ASSERT_TRUE(fr_renderer_compute_init(g,&compute,buffers,12,16,"build/shaders/gas_flags_test.comp.spv",err,sizeof(err))&&fr_renderer_begin(g));
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[4]={64,1,0,0};vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline_layout,0,1,&compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);vkCmdDispatch(g->command,1,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    ASSERT_TRUE(fr_renderer_submit(g));uint32_t *out=stats.mapped;
    printf("Repair flag mask%u updates%u subgroups%u\n",out[FR_GAS_REJECT_STAGE_WORD],out[10],out[11]);
    ASSERT_EQ(out[FR_GAS_REJECT_STAGE_WORD],47u);ASSERT_TRUE(out[11]>0u&&out[10]>0u&&out[10]<=2u*out[11]);
    ASSERT_TRUE(present(g,err,sizeof(err)));fr_renderer_compute_destroy(g,&compute);
    dc_gpu_address_buffer_destroy(g,&model);dc_gpu_address_buffer_destroy(g,&stats);dc_gpu_destroy(g);PASS();
}
#endif
