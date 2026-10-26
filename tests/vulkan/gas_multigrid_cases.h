#ifndef GAS_MULTIGRID_CASES_H
#define GAS_MULTIGRID_CASES_H
static void test_native_multigrid_bounds_work_and_preserves_reference_flow(void) {
    char err[256]={0};const char *names[]={"DC_GAS_MULTIGRID","DC_GAS_TEMPORAL_SOR","DC_GAS_COARSE_WG"};char *saved[3];
    for(uint32_t k=0;k<3;k++) {const char *value=getenv(names[k]);saved[k]=value?SDL_strdup(value):NULL;}
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(4096*sizeof(*reference)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(input&&reference&&out&&volume&&c);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;fr_gas_state_t air=reference_air(m,.0012,300,2);
    double molecular=0;for(uint32_t k=0;k<5;k++) molecular+=air.species[k]*m.gas_constant[k];
    double hot_mass=molecular*300/(m.gas_constant[FR_GAS_EXHAUST]*650);
    for(uint32_t i=0;i<4096;i++) {volume[i]=1;input[i]=air;}
    for(uint32_t y=32;y<40;y++) for(uint32_t x=20;x<28;x++) input[y*64+x]=(fr_gas_state_t){
        .species={0,0,hot_mass,0,0},.energy=hot_mass*m.heat_capacity_cv[FR_GAS_EXHAUST]*650+.5*m.kinetic_scale*hot_mass*4,
        .momentum={2*hot_mass,0},.pigment={.85*hot_mass,.15*hot_mass,.03*hot_mass}};
    uint32_t baseline_work=0,new_work=0;
    for(uint32_t path=0;path<2;path++) {
        ASSERT_EQ(SDL_setenv(names[0],path?"1":"0",1),0);ASSERT_EQ(SDL_setenv(names[1],"0",1),0);ASSERT_EQ(SDL_setenv(names[2],"0",1),0);
        dc_gpu_t *g=NULL;ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_gate(g,29,0,1,64,0,true,err,sizeof(err))&&fr_gas_gpu_gate(g,10,24,20,1,1,true,err,sizeof(err)));
        for(uint32_t step=0;step<12;step++) {
            if(step==8) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,false,err,sizeof(err)));
            g_pressure_dispatches=0;g_direct_dispatches=0;ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
        }
        if(path) new_work=g_pressure_dispatches+g_direct_dispatches;else baseline_work=g_pressure_dispatches+g_direct_dispatches;
        fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock)&&clock.physical_steps==12u&&fabs(clock.advanced_seconds-12.*.02f)<1e-7);
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&present(g,err,sizeof(err)));dc_gpu_destroy(g);
    }
    double mass=0,mass_error=0,color=0,color_error=0,velocity_error=0,velocity_reference=0;
    double before[5]={0},after[5]={0};
    for(uint32_t i=0;i<4096;i++) {
        fr_gas_thermo_t a,b;ASSERT_TRUE(fr_gas_thermodynamics(&m,reference+i,1,&a,err,sizeof(err))&&fr_gas_thermodynamics(&m,out+i,1,&b,err,sizeof(err)));
        ASSERT_TRUE(b.temperature_K>0&&b.temperature_K<5000&&b.pressure<1e5);
        mass+=a.mass;mass_error+=fabs(a.mass-b.mass);
        for(uint32_t k=0;k<5;k++) {before[k]+=input[i].species[k];after[k]+=out[i].species[k];}
        for(uint32_t k=0;k<3;k++) {color+=reference[i].pigment[k];color_error+=fabs(out[i].pigment[k]-reference[i].pigment[k]);}
        for(uint32_t k=0;k<2;k++) {
            double velocity=reference[i].momentum[k]/a.mass,difference=out[i].momentum[k]/b.mass-velocity;
            velocity_reference+=a.mass*velocity*velocity;velocity_error+=a.mass*difference*difference;
        }
    }
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(after[k]-before[k])<fmax(1e-9,before[k]*.003));
    double rms=sqrt(velocity_error/mass),reference_rms=sqrt(velocity_reference/mass);
    printf("Multigrid dispatches%u vs%u mass L1%.9g color L1%.9g velocity RMS%.9g reference%.9g\n",new_work,baseline_work,mass_error/mass,color_error/color,rms,reference_rms);
    ASSERT_TRUE(mass_error/mass<.03&&color_error/color<.10&&rms<fmax(1.,reference_rms*.1));
    ASSERT_TRUE(new_work<100u&&new_work<baseline_work);
    for(uint32_t k=0;k<3;k++) {ASSERT_EQ(SDL_setenv(names[k],saved[k]?saved[k]:"0",1),0);SDL_free(saved[k]);}
    free(input);free(reference);free(out);free(volume);free(c);PASS();
}
#endif
