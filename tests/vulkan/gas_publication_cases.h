#ifndef GAS_PUBLICATION_CASES_H
#define GAS_PUBLICATION_CASES_H
static uint32_t g_publication_submits;
static uint32_t g_pressure_dispatches;
static uint32_t g_direct_dispatches;
static uint32_t g_queue_idle_calls;
static uint64_t g_copy_bytes;
static uint32_t g_transfer_scope_barriers;
VKAPI_ATTR void VKAPI_CALL __real_vkCmdPipelineBarrier2(VkCommandBuffer command,const VkDependencyInfo *dependency);
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdPipelineBarrier2(VkCommandBuffer command,const VkDependencyInfo *dependency) {
    for(uint32_t i=0;i<dependency->memoryBarrierCount;i++)
        if((dependency->pMemoryBarriers[i].dstStageMask&VK_PIPELINE_STAGE_2_TRANSFER_BIT)!=0u) g_transfer_scope_barriers++;
    __real_vkCmdPipelineBarrier2(command,dependency);
}
VKAPI_ATTR void VKAPI_CALL __real_vkCmdCopyBuffer(VkCommandBuffer command,VkBuffer source,VkBuffer destination,uint32_t count,const VkBufferCopy *regions);
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdCopyBuffer(VkCommandBuffer command,VkBuffer source,VkBuffer destination,uint32_t count,const VkBufferCopy *regions) {
    for(uint32_t i=0;i<count;i++) g_copy_bytes+=regions[i].size;
    __real_vkCmdCopyBuffer(command,source,destination,count,regions);
}
VKAPI_ATTR VkResult VKAPI_CALL __real_vkQueueWaitIdle(VkQueue queue);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueWaitIdle(VkQueue queue) {
    g_queue_idle_calls++;return __real_vkQueueWaitIdle(queue);
}
VKAPI_ATTR void VKAPI_CALL __real_vkCmdDispatch(VkCommandBuffer command,uint32_t x,uint32_t y,uint32_t z);
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatch(VkCommandBuffer command,uint32_t x,uint32_t y,uint32_t z) {
    g_direct_dispatches++;__real_vkCmdDispatch(command,x,y,z);
}
VKAPI_ATTR void VKAPI_CALL __real_vkCmdDispatchIndirect(VkCommandBuffer command,VkBuffer buffer,VkDeviceSize offset);
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatchIndirect(VkCommandBuffer command,VkBuffer buffer,VkDeviceSize offset) {
    g_pressure_dispatches++;__real_vkCmdDispatchIndirect(command,buffer,offset);
}
VKAPI_ATTR VkResult VKAPI_CALL __real_vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo *submits,VkFence fence);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo *submits,VkFence fence) {
    g_publication_submits++;return __real_vkQueueSubmit(queue,count,submits,fence);
}
static void test_native_publication_uses_one_submission_and_preserves_rejected_state(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_ROTATE_PUBLISH");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_ROTATE_PUBLISH","1",1),0);
    dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(c&&input&&out&&volume);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;input[i]=reference_air(m,.0012,300,5);
        input[i].species[FR_GAS_NITROGEN]-=.00001;input[i].species[FR_GAS_EXHAUST]=.00001;
        input[i].energy+=.00001*(m.heat_capacity_cv[FR_GAS_EXHAUST]-m.heat_capacity_cv[FR_GAS_NITROGEN])*300;
        if(i%64>=20&&i%64<28&&i/64>=24&&i/64<32) input[i].pigment[0]=.000005;
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.01f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    g_publication_submits=0;ASSERT_TRUE(fr_gas_gpu_step(g,.01f,err,sizeof(err)));
    printf("Practical accepted publication submissions%u\n",g_publication_submits);ASSERT_EQ(g_publication_submits,1u);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&memcmp(input,out,4096*sizeof(*out))!=0);
    memcpy(input,out,4096*sizeof(*out));fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(g,&before));
    ASSERT_TRUE(!fr_gas_gpu_step(g,1e30f,err,sizeof(err)));err[0]=0;
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&memcmp(input,out,4096*sizeof(*out))==0);
    ASSERT_TRUE(fr_gas_gpu_clock(g,&after)&&memcmp(&before,&after,sizeof(before))==0);
    ASSERT_TRUE(fr_gas_gpu_step(g,.01f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    const uint32_t policies[]={FR_GAS_PRESSURE_IMPLICIT,FR_GAS_PRESSURE_HLLC,FR_GAS_PRESSURE_PRACTICAL};
    for(uint32_t k=0;k<3;k++) {
        ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,policies[k],err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
        ASSERT_TRUE(memcmp(input,out,4096*sizeof(*out))==0);
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);free(input);free(out);free(volume);dc_gpu_destroy(g);
    ASSERT_EQ(SDL_setenv("DC_GAS_ROTATE_PUBLISH",saved?saved:"0",1),0);SDL_free(saved);PASS();
}
static void test_native_tick_waits_its_fence_without_draining_presentation_queue(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(g,1.f/60.f)&&dc_gpu_tick_step(g,err,sizeof(err))&&present(g,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(g,&before));
    g_queue_idle_calls=0;ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    printf("Standalone native tick queue-idle calls%u\n",g_queue_idle_calls);ASSERT_EQ(g_queue_idle_calls,0u);
    dc_gpu_tick_capture_t capture={0};g_queue_idle_calls=0;
    ASSERT_TRUE(dc_gpu_tick_capture(g,&capture,err,sizeof(err)));ASSERT_EQ(g_queue_idle_calls,0u);
    ASSERT_TRUE(capture.stages[0].gpu_ns>0&&capture.stages[1].gpu_ns>0&&capture.stages[2].gpu_ns>0);
    ASSERT_TRUE(fr_gas_gpu_clock(g,&after)&&after.physical_steps==before.physical_steps+2u&&
        fabs(after.advanced_seconds-before.advanced_seconds-2.*(1.f/60.f))<1e-7);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_temporal_sor_matches_global_and_bounds_dispatches(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_TEMPORAL_SOR");char *saved=old?SDL_strdup(old):NULL;
    const char *old_mg=getenv("DC_GAS_MULTIGRID");char *saved_mg=old_mg?SDL_strdup(old_mg):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_MULTIGRID","0",1),0);
    const char *old_coarse=getenv("DC_GAS_COARSE_WG");char *saved_coarse=old_coarse?SDL_strdup(old_coarse):NULL;
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(4096*sizeof(*reference)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(input&&reference&&out&&volume&&c);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096;i++) {volume[i]=1;input[i]=reference_air(m,.0012,300,5);}
    for(uint32_t y=24;y<32;y++) for(uint32_t x=20;x<28;x++) {
        fr_gas_state_t *s=input+y*64+x;s->species[FR_GAS_NITROGEN]-=.00001;s->species[FR_GAS_EXHAUST]=.00001;
        s->energy+=.00001*(m.heat_capacity_cv[FR_GAS_EXHAUST]-m.heat_capacity_cv[FR_GAS_NITROGEN])*300;s->pigment[0]=.000005;
    }
    uint32_t baseline_dispatches=0,temporal_dispatches=0,baseline_direct=0,temporal_direct=0;
    for(uint32_t path=0;path<2;path++) {
        ASSERT_EQ(SDL_setenv("DC_GAS_TEMPORAL_SOR",path?"1":"0",1),0);dc_gpu_t *g=NULL;
        ASSERT_EQ(SDL_setenv("DC_GAS_COARSE_WG",path?"1":"0",1),0);
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
        for(uint32_t step=0;step<12;step++) {
            if(step==0||step==8) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,true,err,sizeof(err)));
            if(step==4) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,false,err,sizeof(err)));
            g_pressure_dispatches=0;g_direct_dispatches=0;ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
        }
        if(path) {temporal_dispatches=g_pressure_dispatches;temporal_direct=g_direct_dispatches;}
        else {baseline_dispatches=g_pressure_dispatches;baseline_direct=g_direct_dispatches;}
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&present(g,err,sizeof(err)));dc_gpu_destroy(g);
    }
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(out[i].species[k]-reference[i].species[k])<1e-7);
        for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(out[i].pigment[k]-reference[i].pigment[k])<1e-8);
        for(uint32_t k=0;k<2;k++) ASSERT_TRUE(fabs(out[i].momentum[k]-reference[i].momentum[k])<1e-5);
        ASSERT_TRUE(fabs(out[i].energy-reference[i].energy)<1e-4);
    }
    printf("Temporal pressure dispatches%u vs global%u, exact snapshot%s\n",temporal_dispatches,baseline_dispatches,
        memcmp(out,reference,4096*sizeof(*out))==0?" yes":" no");
    ASSERT_TRUE(temporal_dispatches<70u&&temporal_dispatches<baseline_dispatches);
    printf("Coarse direct dispatches%u vs global%u\n",temporal_direct,baseline_direct);ASSERT_TRUE(temporal_direct<40u&&temporal_direct<baseline_direct);
    ASSERT_EQ(SDL_setenv("DC_GAS_MULTIGRID",saved_mg?saved_mg:"0",1),0);SDL_free(saved_mg);
    ASSERT_EQ(SDL_setenv("DC_GAS_COARSE_WG",saved_coarse?saved_coarse:"0",1),0);SDL_free(saved_coarse);
    ASSERT_EQ(SDL_setenv("DC_GAS_TEMPORAL_SOR",saved?saved:"0",1),0);SDL_free(saved);free(input);free(reference);free(out);free(volume);free(c);PASS();
}
#endif
