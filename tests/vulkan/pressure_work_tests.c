#define _POSIX_C_SOURCE 200809L
#define main fluid_reference_suite_main
#include "fluid_tests.c"
#undef main

static void test_pressure_work_is_empty_then_wakes_for_local_water(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    dc_gpu_t *gpu=make_grid(&left,&right,err,sizeof(err));
    ASSERT_TRUE(gpu);
    bool available=gpu->pressure_work_mapped!=NULL;
    if(!available) {dc_gpu_destroy(gpu);ASSERT_TRUE(available);}
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    const uint32_t *work=gpu->pressure_work_mapped;
    ASSERT_EQ(work[0],0u);ASSERT_EQ(work[1],0u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu,110,20,1,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    ASSERT_TRUE(work[0]>0u&&work[0]<32u);
    ASSERT_EQ(work[1],work[0]);ASSERT_EQ(work[2],1u);ASSERT_EQ(work[3],1u);
    dc_gpu_destroy(gpu);PASS();
}
static bool phase(dc_gpu_t *g,uint32_t mode) {
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if(vkResetCommandBuffer(g->command,0)!=VK_SUCCESS||
        vkBeginCommandBuffer(g->command,&begin)!=VK_SUCCESS) return false;
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        .srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dep={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(g->command,&dep);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    g->fluid_step_scale=1.f;
    dc_gpu_record_fluid_phase(g,mode);
    memory.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    memory.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;
    memory.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(g->command,&dep);
    if(vkEndCommandBuffer(g->command)!=VK_SUCCESS) return false;
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    return vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS&&vkQueueWaitIdle(g->queue)==VK_SUCCESS;
}
static void test_water_inserted_between_pressure_batches_matches_dense_solver(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    left.cells[8*64+8].fluid_mass=DC_FLUID_FULL;
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    dc_gpu_t *sparse=make_grid(&left,&right,err,sizeof(err));ASSERT_TRUE(sparse);
    ASSERT_EQ(setenv("DC_FLUID_VARIANTS","generic",1),0);
    dc_gpu_t *dense=make_grid(&left,&right,err,sizeof(err));
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);ASSERT_TRUE(dense);
    ASSERT_TRUE(phase(sparse,0));ASSERT_TRUE(phase(dense,0));
    ASSERT_TRUE(dc_gpu_paint_material(sparse,110,40,1,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(dense,110,40,1,DC_MATERIAL_WATER,err,sizeof(err)));
    for(uint32_t mode=1;mode<6;mode++) {
        ASSERT_TRUE(phase(sparse,mode));ASSERT_TRUE(phase(dense,mode));
        if(mode==1) {
            const uint32_t *work=sparse->pressure_work_mapped;bool found=false;
            for(uint32_t i=0;i<work[0];i++) found|=work[4+i]==22u;
            ASSERT_TRUE(found);
        }
        for(uint32_t slot=0;slot<2;slot++) {
            dc_chunk_t a,b;
            ASSERT_TRUE(dc_gpu_download_chunk(sparse,slot,&a,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(dense,slot,&b,err,sizeof(err)));
            ASSERT_EQ(memcmp(a.cells,b.cells,sizeof(a.cells)),0);
            ASSERT_EQ(memcmp(a.face_velocity,b.face_velocity,sizeof(a.face_velocity)),0);
        }
    }
    dc_gpu_destroy(sparse);dc_gpu_destroy(dense);PASS();
}
int main(void) {
    RUN(test_pressure_work_is_empty_then_wakes_for_local_water);
    RUN(test_water_inserted_between_pressure_batches_matches_dense_solver);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
