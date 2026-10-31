static void test_hot_gas_heats_each_exposed_wood_face(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    const int dx[4]={0,-1,1,0},dy[4]={-1,0,0,1};
    for(uint32_t face=0;face<4;face++) chunk->cells[20*64+12+face*12]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,900,&wood,err,sizeof(err)));
    for(uint32_t face=0;face<4;face++) {
        wood.parcel_id_low=wood.origin_id_low=900+face;
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,20*64+12+face*12,&wood,err,sizeof(err)));
    }
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,300,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input));double *volumes=calloc(4096,sizeof(*volumes));ASSERT_TRUE(input&&volumes);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volumes[i]=chunk->cells[i].material==DC_MATERIAL_AIR?1:0;
    for(uint32_t face=0;face<4;face++) {
        uint32_t at=(20+dy[face])*64+12+face*12+dx[face];
        input[at].energy*=1100./300.;
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_combustion_gpu_step(g,.001f,err,sizeof(err)));
    float first=0;
    for(uint32_t face=0;face<4;face++) {
        fr_reactive_record_t result;bool found=false;float temperature=0;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+12+face*12,&result,&found,err,sizeof(err))&&found);
        ASSERT_TRUE(fr_reactive_state_temperature(&result.state,&temperature,err,sizeof(err)));
        printf("Hot gas face%u wood %.7fK\n",face,temperature);
        ASSERT_TRUE(temperature>300.0001f);if(face==0) first=temperature;else ASSERT_TRUE(fabsf(temperature-first)<.0001f);
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(volumes);free(chunk);dc_gpu_destroy(g);PASS();
}

static void test_still_air_conducts_heat_without_transporting_mass(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_gas_gpu_enable(g,&model,.0012f,300,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas)),*after=calloc(4096,sizeof(*after));double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(gas&&after&&volume);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volume[i]=1.;
    uint32_t hot=32u*64u+32u,cold=hot+1u;
    gas[hot].energy*=900./300.;
    double before_energy=0,after_energy=0,before_mass=gas[cold].species[FR_GAS_NITROGEN];
    for(uint32_t i=0;i<4096;i++) before_energy+=gas[i].energy;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err))&&fr_combustion_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) after_energy+=after[i].energy;
    double capacity=gas[cold].species[FR_GAS_OXYGEN]*model.heat_capacity_cv[FR_GAS_OXYGEN]+
        before_mass*model.heat_capacity_cv[FR_GAS_NITROGEN];
    double film_temperature=600.;
    double conductivity=5.9776e-6*pow(film_temperature,1.5)/(film_temperature+194.4)*418.4;
    double four_face_heat=4.*conductivity*.0001/.01*.001*(900.-300.);
    ASSERT_TRUE(fabs(after[hot].energy-(gas[hot].energy-four_face_heat))<1e-6);
    ASSERT_TRUE(fabs(after[cold].energy-(gas[cold].energy+four_face_heat/4.))<1e-6);
    ASSERT_TRUE(after[cold].species[FR_GAS_NITROGEN]==gas[cold].species[FR_GAS_NITROGEN]);
    ASSERT_TRUE(fabs((after[cold].energy-gas[cold].energy)/capacity-(four_face_heat/4.)/capacity)<.01);
    ASSERT_TRUE(fabs(after_energy-before_energy)<1e-5);
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err))&&
        fr_gas_gpu_gate(g,32,32,1,1,0,true,err,sizeof(err))&&
        fr_combustion_gpu_step(g,.001f,err,sizeof(err))&&
        fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    ASSERT_TRUE(fabs(after[cold].energy-gas[cold].energy)<1e-7);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(gas);free(after);free(volume);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_partial_body_cell_exchanges_heat_with_its_own_gas(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;chunk->cells[at].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_reactive_state_t wood;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,300,7000,&wood,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,0,at,&wood,err,sizeof(err))&&dc_gpu_admit_authored_rigid_cells(g,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas)),*after=calloc(4096,sizeof(*after));
    double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(gas&&after&&volume);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volume[i]=1.;
    volume[at]=.01;gas[at]=(fr_gas_state_t){0};
    gas[at].species[FR_GAS_NITROGEN]=.00001;
    gas[at].energy=.00001*model.heat_capacity_cv[FR_GAS_NITROGEN]*3000.;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err))&&
        fr_combustion_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    fr_reactive_record_t result;bool found=false;float wood_temperature=0;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,0x80000001u,0,0,&result,&found,err,sizeof(err))&&found&&
        fr_reactive_state_temperature(&result.state,&wood_temperature,err,sizeof(err)));
    printf("Partial-body contact wood %.5fK gas energy %.8g -> %.8gJ\n",
        wood_temperature,gas[at].energy,after[at].energy);
    ASSERT_TRUE(after[at].energy<gas[at].energy&&wood_temperature>300.f);
    double transferred=gas[at].energy-after[at].energy;
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&result.state)-fr_reactive_state_energy(&wood)-transferred)<1e-5);
    free(volume);free(after);free(gas);free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_partly_flooded_cell_exchanges_heat_between_gas_and_water(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t at=20u*64u+20u;chunk->cells[at].material=DC_MATERIAL_AIR;
    chunk->cells[at].fluid_mass=64000u;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,0,0,err,sizeof(err))&&fr_thermal_gpu_enable(g,err,sizeof(err)));
    fr_gas_model_t model={.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
    ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
    fr_gas_state_t *gas=calloc(4096,sizeof(*gas)),*after=calloc(4096,sizeof(*after));
    double *volume=calloc(4096,sizeof(*volume));float *water=calloc(4096,sizeof(*water));
    ASSERT_TRUE(gas&&after&&volume&&water&&fr_gas_gpu_read_chunk(g,0,gas,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) volume[i]=i==at?.0234375:1.;
    gas[at]=(fr_gas_state_t){0};gas[at].species[FR_GAS_NITROGEN]=.00002;
    gas[at].energy=.00002*model.heat_capacity_cv[FR_GAS_NITROGEN]*3000.;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,gas,volume,4096,err,sizeof(err))&&
        fr_combustion_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err))&&
        fr_thermal_gpu_read_water_chunk(g,0,water,4096,err,sizeof(err)));
    printf("Partial-water contact gas %.8g -> %.8gJ water %.8gJ\n",gas[at].energy,after[at].energy,water[at]);
    ASSERT_TRUE(after[at].energy<gas[at].energy&&water[at]>0.f);
    ASSERT_TRUE(fabs((gas[at].energy-after[at].energy)-water[at])<1e-5);
    free(water);free(volume);free(after);free(gas);free(chunk);dc_gpu_destroy(g);PASS();
}
