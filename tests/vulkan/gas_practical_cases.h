#ifndef GAS_PRACTICAL_CASES_H
#define GAS_PRACTICAL_CASES_H
static void test_native_cold_audit_reduces_stock_and_does_not_advance_physics(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;fr_gas_gpu_audit_t audit;
    ASSERT_TRUE(fr_gas_gpu_clock(g,&before)&&fr_gas_gpu_audit(g,&audit,err,sizeof(err))&&fr_gas_gpu_clock(g,&after));
    ASSERT_TRUE(memcmp(&before,&after,sizeof(before))==0);
    ASSERT_TRUE(fabs(audit.metrics[FR_GAS_AUDIT_MASS]-4096.)<.04);
    ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_CELLS],4096.f);ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_INVALID],0.f);
    ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_VELOCITY_BINS],4096.f);ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_MAX_SPEED],0.f);
    ASSERT_TRUE(fabs(audit.metrics[FR_GAS_AUDIT_MIN_TEMPERATURE]-300.)<.001&&fabs(audit.metrics[FR_GAS_AUDIT_MAX_TEMPERATURE]-300.)<.001);
    fr_gas_state_t air=reference_air(m,1,300,0);
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(audit.metrics[FR_GAS_AUDIT_SPECIES+k]-air.species[k]*4096)<.04);
    ASSERT_TRUE(fabs(audit.metrics[FR_GAS_AUDIT_ENERGY]-air.energy*4096)<air.energy*.1);
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*copy=malloc(4096*sizeof(*copy));double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(input&&copy&&volume);
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;input[i]=(fr_gas_state_t){.species={0,0,1,0,0},.energy=m.heat_capacity_cv[FR_GAS_EXHAUST]*650+.5*m.kinetic_scale*400,
            .momentum={20,0},.pigment={.8,.2,.1}};
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_audit(g,&audit,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,copy,4096,err,sizeof(err)));
    ASSERT_TRUE(memcmp(input,copy,4096*sizeof(*input))==0);
    ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_SPECIES+FR_GAS_EXHAUST],4096.f);
    for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(audit.metrics[FR_GAS_AUDIT_PIGMENT+k]-input[0].pigment[k]*4096)<.04);
    ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_MAX_SPEED],20.f);ASSERT_EQ(audit.metrics[FR_GAS_AUDIT_VELOCITY_BINS+3],4096.f);
    ASSERT_TRUE(fr_gas_gpu_clock(g,&after)&&memcmp(&before,&after,sizeof(before))==0);
    free(input);free(copy);free(volume);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_practical_offset_wall_does_not_drive_sealed_neighbor(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(c&&input&&out&&volume);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;input[i]=reference_air(m,.0012,300,0);
        if(i%64>=20&&i%64<28&&i/64>=20&&i/64<28)
            input[i]=(fr_gas_state_t){.species={0,0,.0012,0,0},.energy=.0012*m.heat_capacity_cv[FR_GAS_EXHAUST]*650,.pigment={.0012,0,0}};
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_gate(g,29,0,1,64,0,true,err,sizeof(err)));
    for(uint32_t step=0;step<12;step++) ASSERT_TRUE(fr_gas_gpu_step(g,.05f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    double worst_density=0,escaped=0;
    for(uint32_t i=0;i<4096;i++) if(i%64>=30) {
        fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,1,&t,err,sizeof(err)));
        worst_density=fmax(worst_density,fabs(t.mass/.0012-1.));escaped+=out[i].species[FR_GAS_EXHAUST];
    }
    printf("Offset sealed wall: neighbor density deviation%.9g escaped smoke%.9g\n",worst_density,escaped);
    ASSERT_TRUE(worst_density<.01&&escaped<1e-9&&present(g,err,sizeof(err)));
    free(c);free(input);free(out);free(volume);dc_gpu_destroy(g);PASS();
}
static void test_native_practical_checkerboard_pressure_relaxes_conservatively(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(c&&input&&out&&volume);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;double before=0,variance=0;
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;double density=.0012*(1.+(((i%64+i/64)&1u)? .01:-.01));
        input[i]=reference_air(m,density,300,0);before+=density;variance+=(density-.0012)*(density-.0012);
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    for(uint32_t step=0;step<20;step++) ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    double after=0,remaining=0;
    for(uint32_t i=0;i<4096;i++) {fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,1,&t,err,sizeof(err)));after+=t.mass;remaining+=(t.mass-.0012)*(t.mass-.0012);}
    printf("Checkerboard variance %.9g -> %.9g, mass drift%.9g\n",variance,remaining,after-before);
    ASSERT_TRUE(remaining<variance*.5&&fabs(after-before)<before*.003);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);free(input);free(out);free(volume);dc_gpu_destroy(g);PASS();
}
static void test_native_practical_policy_rejects_unresolved_gravity_without_advancing(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(g,&before));
    ASSERT_TRUE(!fr_gas_gpu_set_gravity(g,900,err,sizeof(err))&&strstr(err,"scale height"));
    ASSERT_TRUE(fr_gas_gpu_clock(g,&after)&&memcmp(&before,&after,sizeof(before))==0&&present(g,err,sizeof(err)));
    free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_practical_variable_intervals_stay_finite_without_retry_stalls(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume)),*work=malloc(4096*sizeof(*work));ASSERT_TRUE(c&&input&&out&&volume&&work);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    fr_gas_state_t air=reference_air(m,.0012,300,0);double specific=0;
    for(uint32_t k=0;k<5;k++) specific+=air.species[k]*m.gas_constant[k]/.0012;
    double hot_mass=.0012*specific*300/(m.gas_constant[FR_GAS_EXHAUST]*650);
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;input[i]=air;
        if(i%64>=12&&i%64<20&&i/64>=40&&i/64<48) {
            input[i]=(fr_gas_state_t){.species={0,0,hot_mass,0,0},.energy=hot_mass*m.heat_capacity_cv[FR_GAS_EXHAUST]*650,
                .pigment={hot_mass*.85,hot_mass*.15,hot_mass*.03}};
        }
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    double stock[5]={0},pigment[3]={0},energy=0,seconds=0,worst=0,ambient_pressure=0;
    fr_gas_thermo_t ambient;ASSERT_TRUE(fr_gas_thermodynamics(&m,&air,1,&ambient,err,sizeof(err)));ambient_pressure=ambient.pressure;
    for(uint32_t i=0;i<4096;i++) {for(uint32_t k=0;k<5;k++) stock[k]+=input[i].species[k];for(uint32_t k=0;k<3;k++) pigment[k]+=input[i].pigment[k];energy+=input[i].energy;}
    const float intervals[4]={1.f/60.f,1.f/30.f,.1f,.25f};
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,true,err,sizeof(err)));
    for(uint32_t step=0;step<120;step++) {
        if(step==60) ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,false,err,sizeof(err)));
        float dt=intervals[step%4u];uint64_t begin=SDL_GetPerformanceCounter();
        bool advanced=fr_gas_gpu_step(g,dt,err,sizeof(err));
        if(!advanced) printf("Practical failed step%u dt%.9g\n",step,dt);
        ASSERT_TRUE(advanced);seconds+=dt;
        double elapsed=(SDL_GetPerformanceCounter()-begin)*1000./SDL_GetPerformanceFrequency();worst=fmax(worst,elapsed);
        ASSERT_TRUE(elapsed<500.);
        if(step%10u==0u||step==119u) {
            ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&dc_gpu_present_chunks(g,err,sizeof(err)));
            for(uint32_t i=0;i<4096;i++) {
                fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,1,&t,err,sizeof(err)));
                if(!(t.temperature_K<5000&&t.pressure<ambient_pressure*10.)) printf("Practical bound step%u cell%u T%.9g pressure%.9g ambient%.9g mass%.9g\n",step,i,t.temperature_K,t.pressure,ambient_pressure,t.mass);
                ASSERT_TRUE(t.temperature_K<5000&&t.pressure<ambient_pressure*10.);
                double speed=hypot(out[i].momentum[0],out[i].momentum[1])/t.mass;ASSERT_TRUE(isfinite(speed)&&speed<1000.);
            }
        }
    }
    fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));ASSERT_EQ(clock.physical_steps,120u);ASSERT_EQ(clock.substeps,120u);ASSERT_TRUE(fabs(clock.advanced_seconds-seconds)<1e-6);
    ASSERT_TRUE(fr_gas_gpu_read_work_chunk(g,0,work,4096,err,sizeof(err)));
    double after[5]={0},colors[3]={0},total_energy=0,external=0;
    for(uint32_t i=0;i<4096;i++) {for(uint32_t k=0;k<5;k++) after[k]+=out[i].species[k];for(uint32_t k=0;k<3;k++) colors[k]+=out[i].pigment[k];total_energy+=out[i].energy;external+=work[i];}
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(after[k]-stock[k])<fmax(1e-9,stock[k]*.003));
    for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(colors[k]-pigment[k])<fmax(1e-9,pigment[k]*.003));
    ASSERT_TRUE(fabs(total_energy-energy-external)<energy*.03);
    printf("Practical120 intervals: advanced%.6fs worst gas call%.3fms energy drift%.9g\n",seconds,worst,total_energy-energy-external);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);free(input);free(out);free(volume);free(work);dc_gpu_destroy(g);PASS();
}
#endif
