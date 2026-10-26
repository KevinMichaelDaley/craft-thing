#ifndef GAS_LOCAL_STAGE_CASES_H
#define GAS_LOCAL_STAGE_CASES_H
static void test_native_local_explicit_stages_preserve_exact_packets_and_clock(void) {
    char err[256]={0};const char *names[]={"DC_GAS_LOCAL_STAGES","DC_GAS_WIDE_SCAN","DC_GAS_FIXED_AXIS","DC_GAS_FUSED_HYBRID"};char *saved[4];
    for(uint32_t k=0;k<4u;k++) {const char *p=getenv(names[k]);saved[k]=p?SDL_strdup(p):NULL;ASSERT_EQ(SDL_setenv(names[k],"1",1),0);}
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(4096*sizeof(*reference)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(chunk&&input&&reference&&out&&volume);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,100);volume[i]=1.;}
    input[36u*64u+36u].energy*=1.00005;uint32_t tiles[2]={0};
    for(uint32_t path=0;path<2u;path++) {
        ASSERT_EQ(SDL_setenv(names[0],path?"1":"0",1),0);dc_gpu_t *g=NULL;
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
            fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err)));
        bool used=false;ASSERT_TRUE(fr_gas_gpu_local_stage_metadata(g,&used));ASSERT_EQ(used,path!=0u);
        ASSERT_TRUE(fr_gas_gpu_pressure_tiles(g,tiles+path)&&fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&present(g,err,sizeof(err)));
        fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock)&&clock.physical_steps==1u&&fabs(clock.advanced_seconds-.02)<1e-8);dc_gpu_destroy(g);
    }
    double mass=0,error=0,stock_before[5]={0},stock_after[5]={0};
    for(uint32_t i=0;i<4096u;i++) {
        fr_gas_thermo_t state;ASSERT_TRUE(fr_gas_thermodynamics(&model,out+i,1,&state,err,sizeof(err))&&state.temperature_K>250&&state.temperature_K<350);
        for(uint32_t k=0;k<5u;k++) {mass+=reference[i].species[k];error+=fabs(out[i].species[k]-reference[i].species[k]);stock_before[k]+=input[i].species[k];stock_after[k]+=out[i].species[k];}
    }
    printf("Local stages pressure tiles%u/%u densityL1%.9g\n",tiles[1],tiles[0],error/mass);
    ASSERT_TRUE(memcmp(out,reference,4096*sizeof(*out))==0&&error/mass==0.);
    for(uint32_t k=0;k<5u;k++) ASSERT_TRUE(fabs(stock_after[k]-stock_before[k])<fmax(1e-10,stock_before[k]*1e-6));
    for(uint32_t k=0;k<4u;k++) {ASSERT_EQ(SDL_setenv(names[k],saved[k]?saved[k]:"0",1),0);SDL_free(saved[k]);}
    free(chunk);free(input);free(reference);free(out);free(volume);PASS();
}
#endif
