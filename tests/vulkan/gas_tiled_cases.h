#ifndef GAS_TILED_CASES_H
#define GAS_TILED_CASES_H
static void test_native_tiled_transport_keeps_stock_clock_and_reference_packet(void) {
    char err[256]={0};dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(4096*sizeof(*reference)),*actual=malloc(4096*sizeof(*actual));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(chunk&&input&&reference&&actual&&volume);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096;i++) {volume[i]=1.;input[i]=reference_air(model,.0012,300,60);}
    for(uint32_t y=20;y<38;y++) for(uint32_t x=18;x<29;x++) {
        fr_gas_state_t *s=input+y*64+x;s->species[1]-=.00001;s->species[2]=.00001;
        s->energy+=.00001*(model.heat_capacity_cv[2]-model.heat_capacity_cv[1])*300;s->pigment[0]=.000007;
    }
    const char *names[]={"DC_GAS_TILED_TRANSPORT","DC_GAS_ROTATE_PUBLISH","DC_GAS_DIRECT_FIRST_AXIS"};char *saved[3];
    for(uint32_t i=0;i<3;i++) {const char *p=getenv(names[i]);saved[i]=p?SDL_strdup(p):NULL;ASSERT_EQ(SDL_setenv(names[i],"1",1),0);}
    uint32_t metadata[2]={0};
    for(uint32_t path=0;path<2;path++) {
        ASSERT_EQ(SDL_setenv(names[0],path?"1":"0",1),0);dc_gpu_t *gpu=NULL;
        ASSERT_TRUE(context(&gpu,err,sizeof(err))&&dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err))&&dc_gpu_set_page(gpu,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(gpu,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(gpu,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(gpu,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(gpu,.02f,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_tiled_metadata(gpu,metadata+path));
        fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(gpu,&clock)&&clock.physical_steps==1u&&fabs(clock.advanced_seconds-.02)<1e-8);
        ASSERT_TRUE(fr_gas_gpu_read_chunk(gpu,0,path?actual:reference,4096,err,sizeof(err))&&present(gpu,err,sizeof(err)));dc_gpu_destroy(gpu);
    }
    double mass=0,error=0,pigment=0,color_error=0,stock[2][5]={{0}};
    for(uint32_t i=0;i<4096;i++) for(uint32_t k=0;k<5;k++) {
        stock[0][k]+=input[i].species[k];stock[1][k]+=actual[i].species[k];mass+=reference[i].species[k];error+=fabs(reference[i].species[k]-actual[i].species[k]);
        if(k<3) {pigment+=reference[i].pigment[k];color_error+=fabs(reference[i].pigment[k]-actual[i].pigment[k]);}
    }
    printf("Tiled route%u/%u densityL1%.9g pigmentL1%.9g\n",metadata[1],metadata[0],error/mass,color_error/pigment);
    ASSERT_TRUE(metadata[0]==0u&&(metadata[1]&4u)!=0u&&((metadata[1]>>8u)&15u)==2u);
    ASSERT_TRUE(error/mass<1e-5&&color_error/pigment<1e-4);
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(stock[0][k]-stock[1][k])<fmax(1e-10,stock[0][k]*1e-5));
    for(uint32_t i=0;i<3;i++) {ASSERT_EQ(SDL_setenv(names[i],saved[i]?saved[i]:"0",1),0);SDL_free(saved[i]);}
    free(chunk);free(input);free(reference);free(actual);free(volume);PASS();
}
#endif
