#ifndef GAS_SUBGRID_BOUNDARY_CASES_H
#define GAS_SUBGRID_BOUNDARY_CASES_H

static void test_native_subgrid_curl_keeps_zero_impulse_at_finish_boundaries(void) {
    enum { SIDE=16, CELL_COUNT=SIDE*SIDE, TILE_COUNT=4 };
    enum { CACHE, MODEL, PAGES, GATES, ACTIVITY, IMPULSE, BUFFER_COUNT };
    const uint32_t bindings[]={3u,5u,7u,11u,21u,24u};
    const VkDeviceSize sizes[]={CELL_COUNT*8u*sizeof(float),FR_GAS_MODEL_WORDS*sizeof(uint32_t),
        sizeof(uint32_t),4096u*sizeof(uint32_t),2u*TILE_COUNT*sizeof(uint32_t),CELL_COUNT*2u*sizeof(float)};
    char err[256]={0};
    dc_gpu_t *gpu=NULL;
    dc_gpu_address_buffer_t buffers[BUFFER_COUNT]={0};
    fr_renderer_compute_t compute={0};
    ASSERT_TRUE(dc_gpu_create(&gpu,64u,64u,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(uint32_t i=0;i<BUFFER_COUNT;++i)
        ASSERT_TRUE(dc_gpu_address_buffer_create(gpu,sizes[i],true,buffers+i,err,sizeof(err)));

    VkDescriptorBufferInfo descriptors[27];
    for(uint32_t i=0;i<27u;++i) descriptors[i]=(VkDescriptorBufferInfo){buffers[GATES].buffer,0,sizes[GATES]};
    for(uint32_t i=0;i<BUFFER_COUNT;++i) descriptors[bindings[i]]=(VkDescriptorBufferInfo){buffers[i].buffer,0,sizes[i]};
    *(uint32_t *)buffers[PAGES].mapped=1u;
    ASSERT_TRUE(fr_renderer_compute_init(gpu,&compute,descriptors,27u,16u,
        "build/shaders/gas_subgrid_boundary_test.comp.spv",err,sizeof(err)));

    float (*cache)[8]=buffers[CACHE].mapped;
    for(uint32_t y=0;y<SIDE;++y) for(uint32_t x=0;x<SIDE;++x) {
        float *cell=cache[y*SIDE+x];
        cell[1]=y==8u?-3.f:(y==5u?.5f:0.f);
        cell[2]=x==8u?2.f:(x==5u?-.5f:0.f);
        cell[6]=1.f;
        cell[7]=1.f;
    }

    const char *cases[]={"complete","sparse pressure publication","empty neighbors"};
    double totals[3][2]={{0}},variation[3]={0};
    uint32_t *activity=buffers[ACTIVITY].mapped;
    for(uint32_t mode=0;mode<3u;++mode) {
        for(uint32_t i=0;i<TILE_COUNT;++i) activity[TILE_COUNT+i]=mode==1u&&i!=0u?0u:2u;
        if(mode==2u) for(uint32_t i=0;i<SIDE;++i) {
            cache[i*SIDE+8u][6]=0.f;
            cache[8u*SIDE+i][6]=0.f;
        }
        ASSERT_TRUE(fr_renderer_begin(gpu));
        fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        uint32_t push[]={SIDE,SIDE,0u,0u};
        const float seconds=.02f;
        memcpy(push+2,&seconds,sizeof(seconds));
        vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline);
        vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute.pipeline_layout,0u,1u,&compute.descriptor,0u,NULL);
        vkCmdPushConstants(gpu->command,compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0u,sizeof(push),push);
        vkCmdDispatch(gpu->command,CELL_COUNT/64u,1u,1u);
        fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
        ASSERT_TRUE(fr_renderer_submit(gpu));
        const float (*impulse)[2]=buffers[IMPULSE].mapped;
        for(uint32_t i=0;i<CELL_COUNT;++i) for(uint32_t axis=0;axis<2u;++axis) {
            totals[mode][axis]+=impulse[i][axis];
            variation[mode]+=fabs(impulse[i][axis]);
        }
        printf("Subgrid %s impulse (%+.9g, %+.9g), local variation %.9g\n",
            cases[mode],totals[mode][0],totals[mode][1],variation[mode]);
    }
    fr_renderer_compute_destroy(gpu,&compute);
    for(uint32_t i=0;i<BUFFER_COUNT;++i) dc_gpu_address_buffer_destroy(gpu,buffers+i);
    dc_gpu_destroy(gpu);
    for(uint32_t mode=0;mode<3u;++mode) {
        ASSERT_TRUE(fabs(totals[mode][0])<1e-6&&fabs(totals[mode][1])<1e-6);
        ASSERT_TRUE(variation[mode]>.01);
    }
    PASS();
}

#endif
