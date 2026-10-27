static void test_oxygen_starvation_and_finite_vented_recovery(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t wood_at=20u*64u+20u,pocket=19u*64u+20u;
    chunk->cells[wood_at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[pocket-1]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[pocket-64]=chunk->cells[pocket-1];
    chunk->cells[wood_at-1]=chunk->cells[pocket-1];
    chunk->cells[wood_at+1]=chunk->cells[pocket-1];
    chunk->cells[wood_at+64]=chunk->cells[pocket-1];
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,1100,300,&wood,err,sizeof(err)));
    wood.flags=FR_REACTIVE_FLAMING;wood.ignition_threshold=.5f;wood.ignition_integral=.6f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,wood_at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*output=calloc(4096,sizeof(*output));double *volumes=calloc(4096,sizeof(*volumes));
    ASSERT_TRUE(input&&output&&volumes);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volumes[i]=chunk->cells[i].material==DC_MATERIAL_AIR?1:0;
    input[pocket]=(fr_gas_state_t){0};input[pocket].species[FR_GAS_EXHAUST]=.0012;
    input[pocket].energy=.0012*model.heat_capacity_cv[FR_GAS_EXHAUST]*293.15;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_gate(g,20,19,1,1,0,true,err,sizeof(err)));
    for(uint32_t i=0;i<4;i++) ASSERT_TRUE(fr_combustion_gpu_step(g,.01f,err,sizeof(err))&&fr_gas_gpu_step(g,.01f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,wood_at,&result,&found,err,sizeof(err))&&found);
    ASSERT_EQ(result.state.fuel_mass_fp,wood.fuel_mass_fp);
    ASSERT_TRUE(fr_gas_gpu_gate(g,20,19,1,1,0,false,err,sizeof(err)));
    for(uint32_t i=0;i<200;i++) ASSERT_TRUE(fr_gas_gpu_step(g,.01f,err,sizeof(err))&&fr_combustion_gpu_step(g,.01f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,wood_at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,output,4096,err,sizeof(err)));
    printf("Vented fuel %u -> %u, pocket oxygen %.9g\n",wood.fuel_mass_fp,result.state.fuel_mass_fp,output[pocket].species[FR_GAS_OXYGEN]);
    ASSERT_TRUE(result.state.fuel_mass_fp<wood.fuel_mass_fp);
    double before=0,after=0;for(uint32_t i=0;i<4096;i++) {before+=input[i].species[FR_GAS_OXYGEN];after+=output[i].species[FR_GAS_OXYGEN];}
    ASSERT_TRUE(after<before&&after>0&&present(g,err,sizeof(err)));
    free(input);free(output);free(volumes);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_fresh_side_air_can_feed_a_surface_with_starved_air_above(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20*64+20;chunk->cells[at]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,300,&wood,err,sizeof(err)));
    wood.flags=FR_REACTIVE_FLAMING;wood.ignition_threshold=.5;wood.ignition_integral=.6;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,293.15f,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volumes=calloc(4096,sizeof(*volumes));ASSERT_TRUE(input&&volumes);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));for(uint32_t i=0;i<4096;i++) volumes[i]=i==at?0:1;
    input[at-64]=(fr_gas_state_t){0};input[at-64].species[FR_GAS_EXHAUST]=.0012;
    input[at-64].energy=.0012*model.heat_capacity_cv[FR_GAS_EXHAUST]*293.15;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_combustion_gpu_step(g,.05f,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,at,&result,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(result.state.fuel_mass_fp<wood.fuel_mass_fp&&present(g,err,sizeof(err)));
    free(input);free(volumes);free(chunk);dc_gpu_destroy(g);PASS();
}
