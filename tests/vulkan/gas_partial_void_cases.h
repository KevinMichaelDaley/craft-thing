#ifndef GAS_PARTIAL_VOID_CASES_H
#define GAS_PARTIAL_VOID_CASES_H
static void check_native_partial_void_contraction(void) {
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
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);free(chunk);free(input);free(out);free(volume);free(before);free(after);
}
static void test_native_partial_void_contraction_displaces_mixture_without_cut_cell_pressure_spikes(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_LOCAL_GEOMETRY");char *saved=old?SDL_strdup(old):NULL;
    int failures=g_fail;
    for(uint32_t local=0;local<2u;local++) {
        ASSERT_EQ(SDL_setenv("DC_GAS_LOCAL_GEOMETRY",local?"1":"0",1),0);check_native_partial_void_contraction();
        if(g_fail!=failures) break;
    }
    ASSERT_EQ(SDL_setenv("DC_GAS_LOCAL_GEOMETRY",saved?saved:"0",1),0);SDL_free(saved);
    if(g_fail==failures) PASS();
}

static void test_repeated_water_void_changes_preserve_closed_exhaust_stock(void) {
    enum { CYCLES = 200, OPEN_VOID = 55173, CONTRACTED_VOID = 55126 };
    const uint32_t channel_void[] = {36391, OPEN_VOID, 13894};
    char err[256] = {0};
    dc_gpu_t *g = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    fr_gas_state_t *input = calloc(DC_CHUNK_CELLS, sizeof(*input));
    double *volume = calloc(DC_CHUNK_CELLS, sizeof(*volume));
    ASSERT_TRUE(chunk && input && volume);
    const fr_gas_model_t model = {
        .gas_constant = {.25984, .29680, .18892, .46152, 0},
        .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
        .kinetic_scale = 1. / 512.,
    };
    /* Each three-cell channel is independent, including its receiving volumes. */
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y) {
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
            uint32_t cell = y * DC_CHUNK_SIDE + x;
            if ((y & 1u) || x % 4u == 3u) {
                chunk->cells[cell].material = DC_MATERIAL_STONE;
                continue;
            }
            uint32_t available = channel_void[x % 4u];
            chunk->cells[cell] = (dc_cell_t){.material = DC_MATERIAL_WATER,
                .fluid_mass = DC_FLUID_FULL - available};
            volume[cell] = (double)available / DC_FLUID_FULL;
            double exhaust = volume[cell] / 64.;
            input[cell].species[FR_GAS_EXHAUST] = exhaust;
            input[cell].energy = exhaust * model.heat_capacity_cv[FR_GAS_EXHAUST] * 300.;
            input[cell].pigment[0] = exhaust / 4.;
        }
    }
    fr_gas_gpu_audit_t before, after;
    ASSERT_TRUE(dc_gpu_create(&g, DC_CHUNK_SIDE, DC_CHUNK_SIDE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk_exact(g, 0, chunk, false, err, sizeof(err)) &&
        dc_gpu_set_page(g, 0, 0, 0, err, sizeof(err)) &&
        fr_gas_gpu_enable(g, &model, .0012f, 300.f, err, sizeof(err)) &&
        fr_gas_gpu_set_pressure_policy(g, FR_GAS_PRESSURE_PRACTICAL, err, sizeof(err)) &&
        fr_gas_gpu_seed_chunk(g, 0, input, volume, DC_CHUNK_CELLS, err, sizeof(err)) &&
        fr_gas_gpu_refresh_geometry(g, err, sizeof(err)) &&
        fr_gas_gpu_audit(g, &before, err, sizeof(err)));
    for (uint32_t edit = 0; edit < 2u * CYCLES; ++edit) {
        uint32_t available = edit % 2u ? OPEN_VOID : CONTRACTED_VOID;
        for (uint32_t y = 0; y < DC_CHUNK_SIDE; y += 2u)
            for (uint32_t x = 1u; x < DC_CHUNK_SIDE; x += 4u)
                chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass = DC_FLUID_FULL - available;
        ASSERT_TRUE(dc_gpu_upload_chunk_exact(g, 0, chunk, false, err, sizeof(err)) &&
            fr_gas_gpu_refresh_geometry(g, err, sizeof(err)));
    }
    fr_gas_gpu_clock_t clock;
    ASSERT_TRUE(fr_gas_gpu_clock(g, &clock) && fr_gas_gpu_audit(g, &after, err, sizeof(err)));
    double drift = (after.species[FR_GAS_EXHAUST] - before.species[FR_GAS_EXHAUST]) * DC_FLUID_FULL;
    printf("Closed geometry exhaust drift %+.9f Q16 across %u water cycles\n", drift, CYCLES);
    dc_gpu_destroy(g);
    free(chunk);
    free(input);
    free(volume);
    ASSERT_EQ(clock.physical_steps, 0u);
    ASSERT_EQ(before.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    ASSERT_EQ(after.metrics[FR_GAS_AUDIT_INVALID], 0.f);
    ASSERT_TRUE(fabs(drift) <= 4.);
    PASS();
}
#endif
