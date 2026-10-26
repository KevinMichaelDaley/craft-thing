#ifndef GAS_GEOMETRY_CACHE_CASES_H
#define GAS_GEOMETRY_CACHE_CASES_H
static void test_native_local_geometry_repairs_keep_quiet_owners_and_avoid_full_cache_rebuilds(void) {
    char err[256]={0};const char *names[]={"DC_GAS_LOCAL_GEOMETRY","DC_GAS_ROTATE_PUBLISH"};char *saved[2];
    for(uint32_t i=0;i<2u;i++) {const char *v=getenv(names[i]);saved[i]=v?SDL_strdup(v):NULL;}
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(4096*sizeof(*reference)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_checkpoint_chunk_t *quiet=malloc(sizeof(*quiet)),*check=malloc(sizeof(*check));ASSERT_TRUE(input&&reference&&out&&volume&&chunk&&quiet&&check);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {volume[i]=1.;input[i]=reference_air(model,.0012,300,2);}
    for(uint32_t i=20u*64u+20u;i<20u*64u+28u;i++) {input[i].species[1]-=.00001;input[i].species[2]=.00001;input[i].pigment[0]=.00001;
        input[i].energy+=.00001*(model.heat_capacity_cv[2]-model.heat_capacity_cv[1])*300;}
    for(uint32_t publication=0;publication<2u;publication++) {
        ASSERT_EQ(SDL_setenv(names[1],publication?"1":"0",1),0);uint32_t work[2]={0};
        for(uint32_t path=0;path<2u;path++) {
            ASSERT_EQ(SDL_setenv(names[0],path?"1":"0",1),0);dc_gpu_t *g=NULL;memset(chunk,0,sizeof(*chunk));
            ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
            ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
                fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
            chunk->coord.x=8;ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err))&&dc_gpu_set_page(g,8,0,1,err,sizeof(err)));
            fr_gas_state_t *still=malloc(4096*sizeof(*still));ASSERT_TRUE(still);
            for(uint32_t i=0;i<4096u;i++) still[i]=reference_air(model,.0012,300,.001);
            ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,1,still,volume,4096,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,quiet,err,sizeof(err)));free(still);
            ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
            for(uint32_t step=0;step<8u;step++) {
                g_direct_dispatches=g_pressure_dispatches=0;
                ASSERT_TRUE(dc_gpu_paint_material(g,32,32,0,(step&1u)?DC_MATERIAL_AIR:DC_MATERIAL_STONE,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err)));
                work[path]+=g_direct_dispatches+g_pressure_dispatches;
            }
            ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,check,err,sizeof(err)));
            ASSERT_EQ(memcmp(quiet->cells,check->cells,sizeof(quiet->cells)),0);ASSERT_EQ(memcmp(quiet->gates,check->gates,sizeof(quiet->gates)),0);
            ASSERT_EQ(check->clock.physical_steps,9u);ASSERT_TRUE(fabs(check->clock.advanced_seconds-.18)<1e-7);
            ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);
        }
        double density_error=0.,mass=0.,energy_error=0.,energy=0.,velocity=0.;
        for(uint32_t i=0;i<4096u;i++) {
            double a=0,b=0;for(uint32_t k=0;k<5u;k++) {a+=reference[i].species[k];b+=out[i].species[k];}
            density_error+=fabs(a-b);mass+=a;energy_error+=fabs(reference[i].energy-out[i].energy);energy+=reference[i].energy;
            for(uint32_t k=0;k<2u;k++) {double delta=(b>0?out[i].momentum[k]/b:0)-(a>0?reference[i].momentum[k]/a:0);velocity+=delta*delta;}
        }
        printf("Local geometry publication%u dispatches%u/%u densityL1%.9g energyL1%.9g velocityRMS%.9g\n",publication,work[1],work[0],density_error/mass,energy_error/energy,sqrt(velocity/8192.));
        ASSERT_TRUE(density_error/mass<.003&&energy_error/energy<.003&&sqrt(velocity/8192.)<.15);
        ASSERT_TRUE(work[1]<work[0]);
    }
    for(uint32_t i=0;i<2u;i++) {SDL_setenv(names[i],saved[i]?saved[i]:"0",1);SDL_free(saved[i]);}
    free(input);free(reference);free(out);free(volume);free(chunk);free(quiet);free(check);PASS();
}
#endif
