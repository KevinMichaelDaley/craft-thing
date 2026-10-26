#ifndef GAS_CHECKPOINT_CASES_H
#define GAS_CHECKPOINT_CASES_H
static void test_native_checkpoint_preserves_compensation_geometry_valves_and_ownership(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_checkpoint_chunk_t *before=malloc(sizeof(*before)),*after=malloc(sizeof(*after));
    fr_gas_state_t *input=malloc(4096*sizeof(*input));double *volume=malloc(4096*sizeof(*volume));
    ASSERT_TRUE(c&&before&&after&&input&&volume&&context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(model,.0012,300,2);volume[i]=.75;}
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&
        fr_gas_gpu_gate(g,24,0,1,64,0,true,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,before,err,sizeof(err)));
    ASSERT_TRUE(before->cells[0][12]==.75f&&before->cells[0][13]==.75f&&before->gates[24]==1u);
    ASSERT_TRUE(fr_gas_gpu_import_checkpoint(g,1,before,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,after,err,sizeof(err)));
    ASSERT_EQ(memcmp(before,after,sizeof(*before)),0);
    before->cells[4095][0]=-1.f;
    ASSERT_TRUE(!fr_gas_gpu_import_checkpoint(g,1,before,err,sizeof(err)));
    before->cells[4095][0]=after->cells[4095][0];
    ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,1,after,err,sizeof(err)));
    ASSERT_EQ(memcmp(before,after,sizeof(*before)),0);
    before->gates[4095]=4u;ASSERT_TRUE(!fr_gas_gpu_import_checkpoint(g,1,before,err,sizeof(err)));
    before->gates[4095]=0u;before->clock.advanced_seconds=INFINITY;
    ASSERT_TRUE(!fr_gas_gpu_import_checkpoint(g,1,before,err,sizeof(err)));before->clock=after->clock;
    ASSERT_TRUE(fr_gas_gpu_step(g,.01f,err,sizeof(err)));
    fr_gas_gpu_clock_t current,reimported;ASSERT_TRUE(fr_gas_gpu_clock(g,&current));
    ASSERT_TRUE(fr_gas_gpu_import_checkpoint(g,1,before,err,sizeof(err))&&fr_gas_gpu_clock(g,&reimported));
    ASSERT_EQ(memcmp(&current,&reimported,sizeof(current)),0);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(c);free(before);free(after);free(input);free(volume);PASS();
}
#endif
