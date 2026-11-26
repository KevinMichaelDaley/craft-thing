#ifndef GAS_PRESSURE_FEEDBACK_CASES_H
#define GAS_PRESSURE_FEEDBACK_CASES_H

static bool seed_grain_feedback_tile(dc_gpu_t *gpu,uint32_t x,uint32_t y,const float sentinel[2]) {
    const uint32_t tile=(y/16u)*((gpu->width+15u)/16u)+x/16u;
    const uint32_t dispatch[]={1u,1u,1u,tile};
    VkDeviceSize offset=((VkDeviceSize)y*gpu->width+x)*4u*sizeof(float)+2u*sizeof(float);
    if(!fr_renderer_begin(gpu)) return false;
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    vkCmdUpdateBuffer(gpu->command,gpu->mpm_activity_buffer,0,sizeof(dispatch),dispatch);
    vkCmdUpdateBuffer(gpu->command,gpu->mpm_force_buffer,offset,2u*sizeof(float),sentinel);
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    return fr_renderer_submit(gpu);
}

static void check_pressure_feedback_transaction(void) {
    enum { X=21,Y=28,CELLS=DC_CHUNK_SIDE*DC_CHUNK_SIDE };
    const uint32_t grain=Y*DC_CHUNK_SIDE+X;
    const float sentinel[]={7.f,-11.f};
    char err[256]={0};
    dc_gpu_t *gpu=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *state=calloc(CELLS,sizeof(*state));
    double *volumes=calloc(CELLS,sizeof(*volumes));
    ASSERT_TRUE(chunk&&state&&volumes);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<CELLS;i++) {
        state[i]=reference_air(model,i==grain-1u?.012:.0012,300.,0.);
        volumes[i]=1.;
    }
    ASSERT_TRUE(dc_gpu_create(&gpu,DC_CHUNK_SIDE,DC_CHUNK_SIDE,"build/shaders/pattern.comp.spv",err,sizeof(err))&&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&dc_gpu_set_page(gpu,0,0,0,err,sizeof(err))&&
        fr_gas_gpu_enable(gpu,&model,.0012f,300.f,err,sizeof(err))&&
        fr_gas_gpu_set_pressure_policy(gpu,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_thermal_gpu_enable(gpu,err,sizeof(err))&&fr_combustion_gpu_enable(gpu,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(gpu,0,state,volumes,CELLS,err,sizeof(err)));
    float *grid=(float *)gpu->mpm_grid_mapped+4u*grain;
    grid[0]=1.f;grid[3]=.25f;
    fr_gas_gpu_geometry_changed(gpu,true);
    ASSERT_TRUE(seed_grain_feedback_tile(gpu,X,Y,sentinel));
    ASSERT_TRUE(fr_gas_gpu_step(gpu,.001f,err,sizeof(err)));
    g_publication_submits=0;
    ASSERT_TRUE(fr_gas_gpu_step(gpu,.001f,err,sizeof(err)));
    uint32_t submits=g_publication_submits;
    ASSERT_TRUE(dc_gpu_mpm_readback_scratch(gpu,err,sizeof(err)));
    const float *force=(const float *)gpu->mpm_force_mapped+4u*grain+2u;
    float accepted_x=force[0];
    ASSERT_TRUE(seed_grain_feedback_tile(gpu,X,Y,sentinel));
    ASSERT_TRUE(!fr_gas_gpu_step(gpu,1e30f,err,sizeof(err)));
    err[0]='\0';
    ASSERT_TRUE(dc_gpu_mpm_readback_scratch(gpu,err,sizeof(err)));
    bool rejected_force_exact=memcmp(force,sentinel,sizeof(sentinel))==0;
    dc_gpu_destroy(gpu);free(volumes);free(state);free(chunk);
    printf("Grain pressure feedback with %u submissions, accepted x %.9g, rejected force exact %u\n",
        submits,accepted_x,rejected_force_exact);
    ASSERT_TRUE(accepted_x>0.f&&accepted_x!=sentinel[0]);
    ASSERT_TRUE(rejected_force_exact);
    ASSERT_EQ(submits,1u);
    PASS();
}

static void test_native_practical_pressure_feedback_keeps_rejected_grain_force(void) {
    char err[64]={0};
    const char *options[]={"DC_GAS_ROTATE_PUBLISH","DC_GAS_LOCAL_GEOMETRY","DC_GAS_FUSED_GEOMETRY"};
    char *saved[sizeof(options)/sizeof(options[0])]={0};
    for(size_t i=0;i<sizeof(saved)/sizeof(saved[0]);++i) {
        const char *old=getenv(options[i]);saved[i]=old?SDL_strdup(old):NULL;
        ASSERT_TRUE(!old||saved[i]);
        ASSERT_EQ(SDL_setenv(options[i],"1",1),0);
    }
    check_pressure_feedback_transaction();
    for(size_t i=0;i<sizeof(saved)/sizeof(saved[0]);++i) {
        int restored=saved[i]?SDL_setenv(options[i],saved[i],1):unsetenv(options[i]);
        SDL_free(saved[i]);
        ASSERT_EQ(restored,0);
    }
}

#endif
