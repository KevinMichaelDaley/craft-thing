static void test_reacting_surface_radiates_even_when_the_wood_core_is_cool(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[650].material=chunk->cells[653].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t cold;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,1100,&cold,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,650,&cold,err,sizeof(err)));
    cold.parcel_id_low=cold.origin_id_low=1101;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,653,&cold,err,sizeof(err))&&
        dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err))&&
        fr_thermal_gpu_mark_bonfire_world(g)&&fr_thermal_gpu_step(g,.001f,err,sizeof(err)));
    fr_reactive_record_t a,b;bool found=false;
    const uint32_t *entry=g->reactive->arena.entries.mapped;
    fr_reactive_handle_t handle={0,entry[FR_ENTRY_GENERATION]};
    ASSERT_TRUE(fr_reactive_arena_read(g,&g->reactive->arena,handle,&a,&found,err,sizeof(err))&&found);
    ASSERT_EQ(a.state.parcel_id_low,1100u);
    /* Isolate the already-solved surface boundary from gas/combustion: both
     * bulk inventories start at 300 K, but the emitting skin is 1200 K. */
    uint32_t skin[]={handle.generation,0,0,0};float hot=1200;
    memcpy(skin+1,&hot,sizeof(hot));
    ASSERT_EQ(vkResetCommandBuffer(g->command,0),VK_SUCCESS);
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ASSERT_EQ(vkBeginCommandBuffer(g->command,&begin),VK_SUCCESS);
    vkCmdUpdateBuffer(g->command,g->reactive->arena.thermal_scratch.buffer,32,sizeof(skin),skin);
    VkMemoryBarrier2 barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask=VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    VkDependencyInfo dependency={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&barrier};
    vkCmdPipelineBarrier2(g->command,&dependency);
    ASSERT_EQ(vkEndCommandBuffer(g->command),VK_SUCCESS);
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    ASSERT_EQ(vkQueueSubmit(g->queue,1,&submit,VK_NULL_HANDLE),VK_SUCCESS);
    ASSERT_EQ(vkQueueWaitIdle(g->queue),VK_SUCCESS);
    double initial=2.*fr_reactive_state_energy(&cold);
    ASSERT_TRUE(fr_thermal_gpu_step(g,.05f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&a,&found,err,sizeof(err))&&found&&
        fr_gpu_reactive_read_body(g,0x80000002u,0,0,&b,&found,err,sizeof(err))&&found);
    double received=fr_reactive_state_energy(&b.state)-fr_reactive_state_energy(&cold);
    printf("Hot-skin radiance delivered %.9g J from finite cool-core inventory\n",received);
    printf("Hot-skin total drift %.9g J\n",fr_reactive_state_energy(&a.state)+fr_reactive_state_energy(&b.state)-initial);
    ASSERT_TRUE(received>.001&&fr_reactive_state_energy(&a.state)<fr_reactive_state_energy(&cold));
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&a.state)+fr_reactive_state_energy(&b.state)-initial)<.01);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
