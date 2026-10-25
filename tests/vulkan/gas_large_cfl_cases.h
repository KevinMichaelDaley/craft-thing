#ifndef GAS_LARGE_CFL_CASES_H
#define GAS_LARGE_CFL_CASES_H
static void test_native_receding_gate_matches_real_reflecting_wall(void) {
    char err[256]={0};fr_gas_model_t m=reference_model();
    fr_gas_state_t *reference=malloc(4096*sizeof(*reference)),*out=malloc(4096*sizeof(*out)),*input=malloc(4096*sizeof(*input));
    double *volume=malloc(4096*sizeof(*volume));dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(reference&&out&&input&&volume&&c);
    for(uint32_t path=0;path<2;path++) {
        dc_gpu_t *g=NULL;
        for(uint32_t i=0;i<4096;i++) {
            bool wall=path==1u&&i%64u>=32u;volume[i]=wall?0:1;
            c->cells[i]=wall?(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN}:(dc_cell_t){0};
            input[i]=wall?(fr_gas_state_t){0}:reference_air(m,1,300,i%64u<32u?-30:0);
        }
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
        if(path==0u) ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,true,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&present(g,err,sizeof(err)));
        dc_gpu_destroy(g);
    }
    for(uint32_t y=0;y<64;y++) ASSERT_EQ(memcmp(reference+y*64u,out+y*64u,32u*sizeof(*out)),0);
    free(reference);free(out);free(input);free(volume);free(c);PASS();
}
static void test_native_large_cfl_subnormal_packet_keeps_one_step_and_closed_stock(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=calloc(4096,sizeof(*out));double *volume=calloc(4096,sizeof(*volume));
    ASSERT_TRUE(c&&input&&out&&volume);fr_gas_model_t m=reference_model();
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t x=0;x<64;x++) {uint32_t i=20u*64u+x;c->cells[i]=(dc_cell_t){0};volume[i]=1;input[i]=reference_air(m,1,300,10+5*sin(x*.3));}
    uint32_t index=20u*64u+20u;input[index].species[FR_GAS_EXHAUST]=(float)1e-38;input[index].pigment[0]=(float)8.5e-39;
    input[index+1u].species[FR_GAS_EXHAUST]=(float)1e-38;input[index+1u].pigment[0]=(float)9.5e-39;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.4f,err,sizeof(err)));
    fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));ASSERT_EQ(clock.substeps,1u);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    double before=0,after=0;for(uint32_t x=0;x<64;x++) {before+=input[20u*64u+x].pigment[0];after+=out[20u*64u+x].pigment[0];}
    ASSERT_TRUE(after>0&&fabs(after-before)<1e-43&&present(g,err,sizeof(err)));
    free(c);free(input);free(out);free(volume);dc_gpu_destroy(g);PASS();
}
static fr_gas_state_t large_air(fr_gas_model_t m,double temperature,double u,double v) {
    fr_gas_state_t s=reference_air(m,.9,temperature,u);s.species[FR_GAS_EXHAUST]=.1;
    s.energy+=.1*m.heat_capacity_cv[FR_GAS_EXHAUST]*temperature+.05*u*u+.5*v*v;
    s.momentum[0]=u;s.momentum[1]=v;return s;
}
static bool large_equal(const fr_gas_state_t *a,const fr_gas_state_t *b,uint32_t count,char *err,uint32_t cap) {
    for(uint32_t i=0;i<count;i++) {
        for(uint32_t k=0;k<5;k++) if(fabs(a[i].species[k]-b[i].species[k])>=2e-5) {
            snprintf(err,cap,"Large-CFL species cell%u channel%u delta%.9g",i,k,b[i].species[k]-a[i].species[k]);return false;
        }
        for(uint32_t k=0;k<3;k++) if(fabs(a[i].pigment[k]-b[i].pigment[k])>=2e-6) {
            snprintf(err,cap,"Large-CFL pigment cell%u channel%u delta%.9g",i,k,b[i].pigment[k]-a[i].pigment[k]);return false;
        }
        for(uint32_t k=0;k<2;k++) if(fabs(a[i].momentum[k]-b[i].momentum[k])>=.0002) {
            snprintf(err,cap,"Large-CFL momentum cell%u channel%u delta%.9g",i,k,b[i].momentum[k]-a[i].momentum[k]);return false;
        }
        if(fabs(a[i].energy-b[i].energy)>=.01) {snprintf(err,cap,"Large-CFL energy cell%u delta%.9g",i,b[i].energy-a[i].energy);return false;}
    }
    return true;
}
static bool large_budget(const fr_gas_state_t *a,const fr_gas_state_t *b,uint32_t count,char *err,uint32_t cap) {
    enum {WORDS=FR_GAS_SPECIES+6};double before[WORDS]={0},after[WORDS]={0};
    for(uint32_t i=0;i<count;i++) {
        double x[WORDS],y[WORDS];memcpy(x,a+i,sizeof(x));memcpy(y,b+i,sizeof(y));
        for(uint32_t k=0;k<WORDS;k++) {before[k]+=x[k];after[k]+=y[k];}
    }
    for(uint32_t k=0;k<WORDS;k++) {
        if(k==6u||k==7u) continue;
        double tolerance=k<5u?.0002:(k==5u?.02:.00002);
        if(fabs(after[k]-before[k])>=tolerance) {snprintf(err,cap,"Large-CFL closed budget channel%u delta%.9g",k,after[k]-before[k]);return false;}
    }
    return true;
}
static void test_native_large_cfl_crosses_chunk_and_subgroup_seams_with_mixed_color(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *chunk=calloc(4096,sizeof(*chunk)),*readback=malloc(4096*sizeof(*readback));double *volume=calloc(4096,sizeof(*volume));
    enum {WIDTH=384};fr_gas_state_t input[WIDTH],expected[WIDTH],out[WIDTH];double v[WIDTH];fr_gas_model_t m=reference_model();ASSERT_TRUE(c&&chunk&&readback&&volume);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t x=0;x<64;x++) {c->cells[20u*64u+x]=(dc_cell_t){0};volume[20u*64u+x]=1;}
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    for(uint32_t slot=0;slot<WIDTH/64u;slot++) ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,c,err,sizeof(err))&&dc_gpu_set_page(g,slot,0,slot,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    for(uint32_t slot=0;slot<WIDTH/64u;slot++) {
        for(uint32_t x=0;x<64;x++) {
            uint32_t global=slot*64u+x,index=20u*64u+x;chunk[index]=large_air(m,300,10,0);
            if(global==316u) chunk[index].pigment[0]=.1;
            if(global==320u) chunk[index].pigment[2]=.1;
        }
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,slot,chunk,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,slot,readback,4096,err,sizeof(err)));
        for(uint32_t x=0;x<64;x++) {input[slot*64u+x]=readback[20u*64u+x];v[slot*64u+x]=1;}
    }
    float dt=.4f;ASSERT_TRUE(fr_gas_large_cfl_reference_step(&m,input,v,WIDTH,1,dt,expected,err,sizeof(err))&&fr_gas_gpu_step(g,dt,err,sizeof(err)));
    for(uint32_t slot=0;slot<WIDTH/64u;slot++) {
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,slot,readback,4096,err,sizeof(err)));
        for(uint32_t x=0;x<64;x++) out[slot*64u+x]=readback[20u*64u+x];
    }
    ASSERT_TRUE(large_equal(expected,out,WIDTH,err,sizeof(err))&&large_budget(input,out,WIDTH,err,sizeof(err)));
    ASSERT_TRUE(out[320].pigment[0]>1e-5&&out[320].pigment[2]>1e-5&&present(g,err,sizeof(err)));
    free(c);free(chunk);free(readback);free(volume);dc_gpu_destroy(g);PASS();
}
static void test_native_large_cfl_closed_valve_matches_two_independent_chambers(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));fr_gas_model_t m=reference_model();
    fr_gas_state_t *chunk=calloc(4096,sizeof(*chunk)),*readback=malloc(4096*sizeof(*readback));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_state_t input[64],expected[64],out[64];double v[64];ASSERT_TRUE(c&&chunk&&readback&&volume);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t x=0;x<64;x++) {uint32_t i=20u*64u+x;c->cells[i]=(dc_cell_t){0};volume[i]=v[x]=1;chunk[i]=large_air(m,300,10,0);}
    chunk[20u*64u+28u].pigment[0]=.1;chunk[20u*64u+32u].pigment[2]=.1;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,chunk,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    for(uint32_t x=0;x<64;x++) input[x]=readback[20u*64u+x];
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,20,1,1,0,true,err,sizeof(err)));
    float dt=.4f;
    ASSERT_TRUE(fr_gas_large_cfl_reference_step(&m,input,v,32,1,dt,expected,err,sizeof(err))&&fr_gas_large_cfl_reference_step(&m,input+32,v+32,32,1,dt,expected+32,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,dt,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    for(uint32_t x=0;x<64;x++) {out[x]=readback[20u*64u+x];if(x<32u) ASSERT_EQ(out[x].pigment[2],0.);else ASSERT_EQ(out[x].pigment[0],0.);}
    ASSERT_TRUE(large_equal(expected,out,64,err,sizeof(err))&&large_budget(input,out,64,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,20,1,1,0,false,err,sizeof(err))&&fr_gas_gpu_step(g,.1f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    double crossed=0;for(uint32_t x=0;x<64;x++) {out[x]=readback[20u*64u+x];if(x>=32u) crossed+=out[x].pigment[0];}
    ASSERT_TRUE(crossed>1e-5&&large_budget(input,out,64,err,sizeof(err))&&present(g,err,sizeof(err)));
    free(c);free(chunk);free(readback);free(volume);dc_gpu_destroy(g);PASS();
}
static void test_native_large_cfl_two_dimensional_concave_wall_matches_oracle(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));fr_gas_model_t m=reference_model();
    fr_gas_state_t *chunk=calloc(4096,sizeof(*chunk)),*readback=malloc(4096*sizeof(*readback));double *volume=calloc(4096,sizeof(*volume));
    fr_gas_state_t input[64]={0},expected[64],out[64];double v[64];ASSERT_TRUE(c&&chunk&&readback&&volume);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t y=0;y<8;y++) for(uint32_t x=0;x<8;x++) {
        uint32_t at=y*8u+x,index=(24u+y)*64u+24u+x;v[at]=x==4u&&y!=4u?0:1;if(!v[at]) continue;
        c->cells[index]=(dc_cell_t){0};volume[index]=1;chunk[index]=large_air(m,3,.5,-.25);
        if(x==2u) chunk[index].pigment[0]=.1;
        if(x==6u) chunk[index].pigment[2]=.1;
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,chunk,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    for(uint32_t y=0;y<8;y++) for(uint32_t x=0;x<8;x++) input[y*8u+x]=readback[(24u+y)*64u+24u+x];
    ASSERT_TRUE(fr_gas_large_cfl_reference_step(&m,input,v,8,8,2,expected,err,sizeof(err))&&fr_gas_gpu_step(g,2,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    for(uint32_t y=0;y<8;y++) for(uint32_t x=0;x<8;x++) out[y*8u+x]=readback[(24u+y)*64u+24u+x];
    ASSERT_TRUE(large_equal(expected,out,64,err,sizeof(err))&&large_budget(input,out,64,err,sizeof(err))&&present(g,err,sizeof(err)));
    free(c);free(chunk);free(readback);free(volume);dc_gpu_destroy(g);PASS();
}
static void test_native_large_cfl_balanced_counterflow_has_no_artificial_compression(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));fr_gas_model_t m=reference_model();
    fr_gas_state_t *chunk=calloc(4096,sizeof(*chunk)),*readback=malloc(4096*sizeof(*readback));double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(c&&chunk&&readback&&volume);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t x=0;x<64;x++) {uint32_t i=20u*64u+x;c->cells[i]=(dc_cell_t){0};volume[i]=1;chunk[i]=reference_air(m,1,300,x%2u?-1:1);}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,chunk,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,chunk,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,4,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,readback,4096,err,sizeof(err)));
    for(uint32_t x=0;x<64;x++) {
        uint32_t i=20u*64u+x;
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(readback[i].species[k]-chunk[i].species[k])<2e-6);
        ASSERT_TRUE(fabs(readback[i].energy-chunk[i].energy)<.005);
        ASSERT_TRUE(fabs(readback[i].momentum[0]-(x%2u?-1.:1.)/9.)<.0002);
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);free(chunk);free(readback);free(volume);dc_gpu_destroy(g);PASS();
}
#endif
