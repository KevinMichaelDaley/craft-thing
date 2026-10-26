#ifndef GAS_PARTIAL_VOID_CASES_H
#define GAS_PARTIAL_VOID_CASES_H
static void test_native_partial_void_contraction_displaces_mixture_without_cut_cell_pressure_spikes(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_checkpoint_chunk_t *before=malloc(sizeof(*before)),*after=malloc(sizeof(*after));ASSERT_TRUE(chunk&&input&&out&&volume&&before&&after);
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096u;i++) {
        input[i]=reference_air(model,.0012,300,0);volume[i]=1.;input[i].species[1]-=.00001;input[i].species[2]=.00001;input[i].pigment[0]=.000005;
        input[i].energy+=.00001*(model.heat_capacity_cv[2]-model.heat_capacity_cv[1])*300;
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&
        fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,0,before,err,sizeof(err)));
    const uint32_t index=20u*64u+20u,water[2]={DC_FLUID_FULL/2u,DC_FLUID_FULL-1u};
    for(uint32_t stage=0;stage<2u;stage++) {
        chunk->cells[index]=(dc_cell_t){.material=DC_MATERIAL_WATER,.fluid_mass=water[stage]};
        ASSERT_TRUE(dc_gpu_upload_chunk_exact(g,0,chunk,false,err,sizeof(err))&&fr_gas_gpu_refresh_geometry(g,err,sizeof(err))&&
            fr_gas_gpu_export_checkpoint(g,0,after,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
        double available=1.-(double)water[stage]/DC_FLUID_FULL;fr_gas_thermo_t state;
        ASSERT_TRUE(fr_gas_thermodynamics(&model,out+index,available,&state,err,sizeof(err)));
        printf("Partial void%.9g mass%.9g density%.9g temperature%.9g\n",available,state.mass,state.mass/available,state.temperature_K);
        ASSERT_TRUE(fabs(state.mass/available-.0012)<.0012*1e-5&&fabs(state.temperature_K-300)<.001);
        ASSERT_TRUE(fabs(out[index].pigment[0]/available-.000005)<.000005*1e-5);
        for(uint32_t k=0;k<5u;k++) ASSERT_TRUE(fabs(gas_checkpoint_stock(after,k)-gas_checkpoint_stock(before,k))<1e-7);
        for(uint32_t k=8u;k<11u;k++) ASSERT_TRUE(fabs(gas_checkpoint_stock(after,k)-gas_checkpoint_stock(before,k))<1e-9);
        double old_energy=0,new_energy=0;
        for(uint32_t i=0;i<4096u;i++) {old_energy+=(double)before->cells[i][5]+before->cells[i][11]+before->cells[i][14];new_energy+=(double)after->cells[i][5]+after->cells[i][11]+after->cells[i][14];}
        ASSERT_TRUE(fabs(new_energy-old_energy)<old_energy*1e-6&&after->clock.physical_steps==1u);
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(chunk);free(input);free(out);free(volume);free(before);free(after);PASS();
}
#endif
