static void test_ordinary_gpu_oxygen_response_is_continuous_and_stoichiometric(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,2100,&wood,err,sizeof(err)));
    wood.flags|=FR_REACTIVE_FLAMING;
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},.heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.01f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*before=calloc(4096,sizeof(*before)),*after=calloc(4096,sizeof(*after));
    double *volumes=calloc(4096,sizeof(*volumes));ASSERT_TRUE(input&&before&&after&&volumes);
    const double fractions[6]={0,.02,.119,.12,.121,.21};uint32_t reacted[6]={0};
    for(uint32_t run=0;run<6;run++) {
        for(uint32_t i=0;i<4096;i++) {
            volumes[i]=i==at?0:1;input[i]=(fr_gas_state_t){0};if(i==at) continue;
            input[i].species[FR_GAS_OXYGEN]=.01*fractions[run];input[i].species[FR_GAS_NITROGEN]=.01*(1-fractions[run]);
            input[i].energy=(input[i].species[FR_GAS_OXYGEN]*model.heat_capacity_cv[FR_GAS_OXYGEN]+
                input[i].species[FR_GAS_NITROGEN]*model.heat_capacity_cv[FR_GAS_NITROGEN])*293.15;
        }
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err))&&fr_combustion_gpu_step(g,.25f,err,sizeof(err)));
        fr_reactive_record_t record;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&record,&found,err,sizeof(err))&&found&&fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
        reacted[run]=wood.fuel_mass_fp-record.state.fuel_mass_fp;double oxygen=0;
        for(uint32_t i=0;i<4096;i++) {ASSERT_TRUE(after[i].species[FR_GAS_OXYGEN]>=0);oxygen+=before[i].species[FR_GAS_OXYGEN]-after[i].species[FR_GAS_OXYGEN];}
        printf("Oxygen fraction%.4f reacted%u consumed%.9g\n",fractions[run],reacted[run],oxygen);fflush(stdout);
        ASSERT_TRUE(fabs(oxygen-reacted[run]/65536.*1.2)<1e-7);
        if(run==0) {ASSERT_EQ(reacted[run],0);ASSERT_TRUE(record.state.flags&FR_REACTIVE_FLAMING);}
        else ASSERT_TRUE(reacted[run]>0);
    }
    ASSERT_TRUE(reacted[2]<=reacted[3]&&reacted[3]<=reacted[4]&&reacted[4]-reacted[2]<3u);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(before);free(after);free(volumes);free(chunk);dc_gpu_destroy(g);PASS();
}
