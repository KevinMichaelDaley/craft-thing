#ifndef GAS_GEOMETRY_CACHE_CASES_H
#define GAS_GEOMETRY_CACHE_CASES_H
static void test_native_fused_geometry_keeps_edited_stock_and_removes_completion_wait(void) {
    char err[256]={0};const char *names[]={"DC_GAS_FUSED_GEOMETRY","DC_GAS_LOCAL_GEOMETRY","DC_GAS_ROTATE_PUBLISH"};char *saved[3];
    for(uint32_t k=0;k<3u;k++) {const char *p=getenv(names[k]);saved[k]=p?SDL_strdup(p):NULL;SDL_setenv(names[k],"1",1);}
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_gas_state_t *input=malloc(4096*sizeof(*input));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_checkpoint_chunk_t *reference=malloc(sizeof(*reference)),*out=malloc(sizeof(*out)),*quiet=malloc(sizeof(*quiet)),*check=malloc(sizeof(*check));
    ASSERT_TRUE(chunk&&input&&volume&&reference&&out&&quiet&&check);fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    uint32_t submissions[2]={0};
    for(uint32_t path=0;path<2u;path++) {
        SDL_setenv(names[0],path?"1":"0",1);dc_gpu_t *g=NULL;memset(chunk,0,sizeof(*chunk));
        for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,60);volume[i]=1.;}
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
        ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
        chunk->coord.x=8;ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err))&&dc_gpu_set_page(g,8,0,1,err,sizeof(err)));
        for(uint32_t i=0;i<4096u;i++) input[i]=reference_air(model,.0012,300,.001);
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,1,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,quiet,err,sizeof(err)));
        chunk->coord.x=0;ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));
        for(uint32_t step=0;step<8u;step++) {
            chunk->cells[32u*64u+32u]=(dc_cell_t){.material=(step&1u)?DC_MATERIAL_AIR:DC_MATERIAL_STONE,.flags=(step&1u)?0u:DC_CELL_LOADED_TERRAIN};
            ASSERT_TRUE(dc_gpu_upload_chunk_exact(g,0,chunk,false,err,sizeof(err)));
            g_publication_submits=0;ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err)));submissions[path]+=g_publication_submits;
        }
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,path?out:reference,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,check,err,sizeof(err)));
        ASSERT_EQ(memcmp(quiet->cells,check->cells,sizeof(quiet->cells)),0);
        ASSERT_EQ(memcmp(quiet->carrier_tail,check->carrier_tail,sizeof(quiet->carrier_tail)),0);
        ASSERT_EQ(check->clock.physical_steps,9u);
        ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);
    }
    bool exact=memcmp(reference->cells,out->cells,sizeof(out->cells))==0&&
        memcmp(reference->carrier_tail,out->carrier_tail,sizeof(out->carrier_tail))==0;
    printf("Fused geometry submissions%u/%u exact packet%s\n",submissions[1],submissions[0],exact?" yes":" no");
    ASSERT_TRUE(exact);ASSERT_EQ(memcmp(&reference->clock,&out->clock,sizeof(out->clock)),0);
    ASSERT_TRUE(submissions[1]==8u&&submissions[0]==16u);
    for(uint32_t k=0;k<3u;k++) {SDL_setenv(names[k],saved[k]?saved[k]:"0",1);SDL_free(saved[k]);}
    free(chunk);free(input);free(volume);free(reference);free(out);free(quiet);free(check);PASS();
}
static void check_native_fused_physics_tick(void) {
    enum { SHOCK_CELL=20u*DC_CHUNK_SIDE+24u,SHOCK_PARCEL=2906u };
    char err[256]={0};
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));fr_gas_state_t *input=malloc(4096*sizeof(*input));double *volume=malloc(4096*sizeof(*volume));
    fr_gas_checkpoint_chunk_t *reference=malloc(sizeof(*reference)),*out=malloc(sizeof(*out)),*quiet=malloc(sizeof(*quiet)),*check=malloc(sizeof(*check));
    ASSERT_TRUE(chunk&&input&&volume&&reference&&out&&quiet&&check);fr_gas_model_t model=reference_model();model.kinetic_scale=1./512.;
    uint32_t submissions[2]={0};
    float shocked_energy[2]={0};
    fr_reactive_state_t shocked;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536u,0u,500.f,SHOCK_PARCEL,&shocked,err,sizeof(err)));
    shocked.pending_shock_energy=1.5f;
    for(uint32_t path=0;path<2u;path++) {
        ASSERT_EQ(SDL_setenv("DC_GAS_FUSED_TICK",path?"1":"0",1),0);
        dc_gpu_t *g=NULL;memset(chunk,0,sizeof(*chunk));
        for(uint32_t i=0;i<4096u;i++) {input[i]=reference_air(model,.0012,300,60);volume[i]=1.;}
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&model,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
        ASSERT_TRUE(fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_combustion_gpu_enable(g,err,sizeof(err)));
        chunk->coord.x=8;ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err))&&dc_gpu_set_page(g,8,0,1,err,sizeof(err)));
        for(uint32_t i=0;i<4096u;i++) input[i]=reference_air(model,.0012,300,.001);
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,1,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,quiet,err,sizeof(err)));
        bool handled=true;
        ASSERT_TRUE(fr_gas_gpu_try_fused_tick(g,.02f,&handled,err,sizeof(err))&&!handled);
        chunk->coord.x=0;ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err))&&dc_gpu_set_tick_seconds(g,.02f));
        chunk->cells[SHOCK_CELL]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
        for(uint32_t step=0;step<8u;step++) {
            chunk->cells[32u*64u+32u]=(dc_cell_t){.material=(step&1u)?DC_MATERIAL_AIR:DC_MATERIAL_STONE,.flags=(step&1u)?0u:DC_CELL_LOADED_TERRAIN};
            ASSERT_TRUE(dc_gpu_upload_chunk_exact(g,0,chunk,false,err,sizeof(err)));
            shocked.parcel_id_low=shocked.origin_id_low=SHOCK_PARCEL+step;
            ASSERT_TRUE(fr_gpu_reactive_seed_cell(g,0,SHOCK_CELL,&shocked,err,sizeof(err)));
            g_publication_submits=0;ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));submissions[path]+=g_publication_submits;
        }
        fr_reactive_record_t thermal;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,SHOCK_CELL,&thermal,&found,err,sizeof(err))&&found);
        ASSERT_EQ(thermal.state.pending_shock_energy,0.f);
        shocked_energy[path]=thermal.state.thermal_energy;
        ASSERT_TRUE(fr_gas_gpu_export_checkpoint(g,0,path?out:reference,err,sizeof(err))&&fr_gas_gpu_export_checkpoint(g,1,check,err,sizeof(err)));
        ASSERT_EQ(memcmp(quiet->cells,check->cells,sizeof(quiet->cells)),0);
        ASSERT_EQ(memcmp(quiet->carrier_tail,check->carrier_tail,sizeof(quiet->carrier_tail)),0);
        ASSERT_EQ(check->clock.physical_steps,9u);
        ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);
    }
    bool exact=memcmp(reference->cells,out->cells,sizeof(out->cells))==0&&
        memcmp(reference->carrier_tail,out->carrier_tail,sizeof(out->carrier_tail))==0;
    printf("Coupled tick submissions%u/%u exact packet %u, heated wood %.9g/%.9g\n",
        submissions[1],submissions[0],exact,shocked_energy[1],shocked_energy[0]);
    ASSERT_EQ(shocked_energy[1],shocked_energy[0]);ASSERT_TRUE(exact);
    ASSERT_EQ(memcmp(&reference->clock,&out->clock,sizeof(out->clock)),0);
    ASSERT_TRUE(submissions[1]==8u&&submissions[0]==16u);
    free(chunk);free(input);free(volume);free(reference);free(out);free(quiet);free(check);PASS();
}
static void test_native_fused_physics_tick_keeps_packets_clock_and_removes_completion_wait(void) {
    char err[64]={0};
    const struct {const char *name,*value;} options[]={
        {"DC_GAS_FUSED_TICK","1"},{"DC_GAS_LOCAL_GEOMETRY","1"},
        {"DC_GAS_ROTATE_PUBLISH","1"},{"DC_GAS_FUSED_GEOMETRY","1"},{"DC_FRAME_TIMINGS","0"}
    };
    char *saved[sizeof(options)/sizeof(options[0])]={0};
    const size_t count=sizeof(saved)/sizeof(saved[0]);
    for(size_t i=0;i<count;++i) {
        const char *old=getenv(options[i].name);saved[i]=old?SDL_strdup(old):NULL;
        ASSERT_TRUE(!old||saved[i]);
        ASSERT_EQ(SDL_setenv(options[i].name,options[i].value,1),0);
    }
    check_native_fused_physics_tick();
    for(size_t i=0;i<count;++i) {
        int restored=saved[i]?SDL_setenv(options[i].name,saved[i],1):unsetenv(options[i].name);
        SDL_free(saved[i]);
        ASSERT_EQ(restored,0);
    }
}
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
        ASSERT_EQ(SDL_setenv(names[1],publication?"1":"0",1),0);uint32_t work[2]={0},submits[2]={0};uint64_t copies[2]={0};
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
                g_direct_dispatches=g_pressure_dispatches=g_publication_submits=0;g_copy_bytes=0;
                ASSERT_TRUE(dc_gpu_paint_material(g,32,32,0,(step&1u)?DC_MATERIAL_AIR:DC_MATERIAL_STONE,err,sizeof(err))&&fr_gas_gpu_step(g,.02f,err,sizeof(err)));
                work[path]+=g_direct_dispatches+g_pressure_dispatches;
                submits[path]+=g_publication_submits;copies[path]+=g_copy_bytes;
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
        printf("Local geometry publication%u dispatches%u/%u submits%u/%u copies%llu/%llu densityL1%.9g energyL1%.9g velocityRMS%.9g\n",publication,work[1],work[0],submits[1],submits[0],(unsigned long long)copies[1],(unsigned long long)copies[0],density_error/mass,energy_error/energy,sqrt(velocity/8192.));
        ASSERT_TRUE(density_error/mass<.003&&energy_error/energy<.003&&sqrt(velocity/8192.)<.15);
        ASSERT_TRUE(submits[1]<submits[0]);if(!publication) ASSERT_TRUE(copies[1]<copies[0]);
    }
    for(uint32_t i=0;i<2u;i++) {SDL_setenv(names[i],saved[i]?saved[i]:"0",1);SDL_free(saved[i]);}
    free(input);free(reference);free(out);free(volume);free(chunk);free(quiet);free(check);PASS();
}
#endif
