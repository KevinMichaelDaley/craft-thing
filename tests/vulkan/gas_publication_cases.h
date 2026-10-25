#ifndef GAS_PUBLICATION_CASES_H
#define GAS_PUBLICATION_CASES_H
static uint32_t g_publication_submits;
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
#endif
