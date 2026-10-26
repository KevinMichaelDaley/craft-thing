#ifndef GAS_MULTIGRID_CASES_H
#define GAS_MULTIGRID_CASES_H
#include "../../src/vulkan/gas_gpu_internal.h"
static void check_native_gas_dispatch_fusion(const char *setting,bool metadata,uint32_t added_dispatches) {
    char err[256]={0};const char *names[]={"DC_GAS_MULTIGRID",setting,"DC_GAS_FUSED_AXIS","DC_GAS_TEMPORAL_SOR"};char *saved[4];
    bool direct_first=strcmp(setting,"DC_GAS_DIRECT_FIRST_AXIS")==0,quiet_publication=strcmp(setting,"DC_GAS_ROTATE_PUBLISH")==0;
    bool cached_neighbors=strcmp(setting,"DC_GAS_CACHED_NEIGHBORS")==0;
    bool lean_metadata=strcmp(setting,"DC_GAS_LEAN_METADATA")==0;
    bool all_slots=direct_first||quiet_publication||cached_neighbors||lean_metadata;uint32_t field_count=all_slots?12288u:4096u;
    for(uint32_t k=0;k<4;k++) {const char *value=getenv(names[k]);saved[k]=value?SDL_strdup(value):NULL;}
    ASSERT_EQ(SDL_setenv(names[0],"1",1),0);
    ASSERT_EQ(SDL_setenv(names[2],"1",1),0);
    ASSERT_EQ(SDL_setenv(names[3],"1",1),0);
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*reference=malloc(field_count*sizeof(*reference)),*out=malloc(field_count*sizeof(*out));
    fr_gas_state_t *dormant=all_slots?malloc(4096*sizeof(*dormant)):NULL;ASSERT_TRUE(!all_slots||dormant);
    double *volume=malloc(4096*sizeof(*volume));dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(input&&reference&&out&&volume&&c);
    fr_gas_state_t *quiet=quiet_publication||cached_neighbors||lean_metadata?malloc(4096*sizeof(*quiet)):NULL;
    ASSERT_TRUE(!(quiet_publication||cached_neighbors||lean_metadata)||quiet);
    fr_gas_model_t m=reference_model();m.kinetic_scale=1./512.;
    if(quiet) for(uint32_t i=0;i<4096;i++) quiet[i]=reference_air(m,.0012,300,.001);
    for(uint32_t i=0;i<4096;i++) {volume[i]=1;input[i]=reference_air(m,.0012,300,
        cached_neighbors?0:(all_slots||strcmp(setting,"DC_GAS_FUSED_HYBRID")==0||strcmp(setting,"DC_GAS_FIXED_AXIS")==0?100:(metadata?40:2)));}
    for(uint32_t y=24;y<32;y++) for(uint32_t x=20;x<28;x++) {
        fr_gas_state_t *s=input+y*64+x;s->species[1]-=.00001;s->species[2]=.00001;
        s->energy+=.00001*(m.heat_capacity_cv[2]-m.heat_capacity_cv[1])*300;s->pigment[0]=.000005;
    }
    uint32_t work[2]={0},tiles[2]={0},widths[2]={0},transfer_barriers[2]={0},repeated[2]={0};float rates[2][3]={{0}};uint64_t copy_bytes[2]={0};
    uint32_t activity_modes[2]={0};
    bool empty_tiles=strcmp(setting,"DC_GAS_SKIP_EMPTY_PRESSURE")==0,group_rate=strcmp(setting,"DC_GAS_GROUP_RATE")==0;
    uint32_t requested_width=group_rate&&saved[1]&&strcmp(saved[1],"256")==0?256u:128u;
    for(uint32_t path=0;path<2;path++) {
        ASSERT_EQ(SDL_setenv(names[1],path?(group_rate&&requested_width==256u?"256":"1"):"0",1),0);dc_gpu_t *g=NULL;
        ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_enable(g,&m,.0012,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_PRACTICAL,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,cached_neighbors?0:900,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
        if(all_slots) ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,2,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,2,dormant,4096,err,sizeof(err)));
        for(uint32_t step=0;step<8;step++) {
            if((empty_tiles||all_slots)&&step==4u) {
                c->coord.x=quiet?8:1;
                ASSERT_TRUE(dc_gpu_upload_chunk(g,1,c,err,sizeof(err))&&dc_gpu_set_page(g,c->coord.x,0,1,err,sizeof(err))&&
                    fr_gas_gpu_seed_chunk(g,1,quiet?quiet:input,volume,4096,err,sizeof(err)));c->coord.x=0;
                if(quiet) ASSERT_TRUE(fr_gas_gpu_read_chunk(g,1,quiet,4096,err,sizeof(err)));
            }
            if(step==0||step==6) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,true,err,sizeof(err)));
            if(step==3) ASSERT_TRUE(fr_gas_gpu_gate(g,29,0,1,64,0,false,err,sizeof(err)));
            g_pressure_dispatches=0;g_direct_dispatches=0;g_copy_bytes=0;g_transfer_scope_barriers=0;
            g_previous_compute_pipeline=VK_NULL_HANDLE;g_repeated_compute_bindings=0;
            ASSERT_TRUE(fr_gas_gpu_step(g,metadata?.04f:.02f,err,sizeof(err)));
            if(cached_neighbors&&step==0u) ASSERT_TRUE(fr_gas_gpu_activity_metadata(g,activity_modes+path));
        }
        work[path]=g_pressure_dispatches+g_direct_dispatches;
        copy_bytes[path]=g_copy_bytes;
        transfer_barriers[path]=g_transfer_scope_barriers;
        repeated[path]=g_repeated_compute_bindings;
        fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock)&&clock.physical_steps==8u&&fabs(clock.advanced_seconds-8.*(metadata?.04f:.02f))<1e-7);
        if(group_rate||all_slots) ASSERT_TRUE(fr_gas_gpu_rate_metadata(g,rates[path],widths+path));
        if(empty_tiles) ASSERT_TRUE(fr_gas_gpu_pressure_tiles(g,tiles+path));
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,path?out:reference,4096,err,sizeof(err))&&present(g,err,sizeof(err)));
        if(all_slots) {
            fr_gas_state_t *destination=path?out:reference;
            ASSERT_TRUE(fr_gas_gpu_read_chunk(g,1,destination+4096,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,2,destination+8192,4096,err,sizeof(err)));
            ASSERT_TRUE(memcmp(destination+8192,dormant,4096*sizeof(*dormant))==0);
            if(quiet) ASSERT_TRUE(memcmp(destination+4096,quiet,4096*sizeof(*quiet))==0);
        }
        dc_gpu_destroy(g);
    }
    printf("%s dispatches%u vs%u exact snapshot%s\n",setting,work[1],work[0],memcmp(out,reference,field_count*sizeof(*out))==0?" yes":" no");
    if(strcmp(setting,"DC_GAS_HALF_PRESSURE")==0) {
        double mass=0,mass_error=0,pigment=0,pigment_error=0,velocity_error=0,stock_before[5]={0},stock_after[5]={0};
        for(uint32_t i=0;i<4096;i++) {
            fr_gas_thermo_t a,b;ASSERT_TRUE(fr_gas_thermodynamics(&m,reference+i,1,&a,err,sizeof(err))&&fr_gas_thermodynamics(&m,out+i,1,&b,err,sizeof(err)));
            ASSERT_TRUE(b.temperature_K>0&&b.temperature_K<5000&&b.pressure<1e5);
            mass+=a.mass;mass_error+=fabs(a.mass-b.mass);
            for(uint32_t k=0;k<5;k++) {stock_before[k]+=input[i].species[k];stock_after[k]+=out[i].species[k];}
            for(uint32_t k=0;k<3;k++) {pigment+=reference[i].pigment[k];pigment_error+=fabs(out[i].pigment[k]-reference[i].pigment[k]);}
            for(uint32_t k=0;k<2;k++) {double difference=out[i].momentum[k]/b.mass-reference[i].momentum[k]/a.mass;velocity_error+=a.mass*difference*difference;}
        }
        printf("Compressed pressure densityL1%.9g pigmentL1%.9g velocityRMS%.9g\n",mass_error/mass,pigment_error/pigment,sqrt(velocity_error/mass));
        ASSERT_TRUE(mass_error/mass<.03&&pigment_error/pigment<.10&&sqrt(velocity_error/mass)<1.);
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(stock_after[k]-stock_before[k])<fmax(1e-9,.003*stock_before[k]));
    } else ASSERT_TRUE(memcmp(out,reference,field_count*sizeof(*out))==0);
    ASSERT_TRUE(added_dispatches==UINT32_MAX?work[1]==work[0]:(added_dispatches?work[1]==work[0]+added_dispatches:work[1]<work[0]));
    if(group_rate) {
        printf("CFL reduction width%u vs%u rates %.9g %.9g %.9g\n",widths[1],widths[0],rates[1][0],rates[1][1],rates[1][2]);
        ASSERT_TRUE(widths[0]==0u&&widths[1]==requested_width&&memcmp(rates[0],rates[1],sizeof(rates[0]))==0);
    }
    if(direct_first) {
        printf("First-axis GPU copy bytes%llu vs%llu\n",(unsigned long long)copy_bytes[1],(unsigned long long)copy_bytes[0]);
        ASSERT_TRUE(copy_bytes[1]<copy_bytes[0]&&memcmp(rates[0],rates[1],sizeof(rates[0]))==0);
    }
    if(empty_tiles) {printf("Pressure tile work%u vs%u\n",tiles[1],tiles[0]);ASSERT_TRUE(tiles[1]<tiles[0]);}
    if(strcmp(setting,"DC_GAS_COMPUTE_BARRIERS")==0) {
        printf("Transfer destination barrier scopes%u vs%u\n",transfer_barriers[1],transfer_barriers[0]);
        ASSERT_TRUE(transfer_barriers[1]<transfer_barriers[0]);
    }
    if(strcmp(setting,"DC_GAS_FIXED_AXIS")==0) {
        printf("Repeated identical compute pipeline bindings%u vs%u\n",repeated[1],repeated[0]);
        ASSERT_TRUE(repeated[1]+1u==repeated[0]);
    }
    if(quiet_publication) ASSERT_TRUE(memcmp(rates[0],rates[1],sizeof(rates[0]))==0&&memcmp(out,input,4096*sizeof(*out))!=0);
    if(lean_metadata) ASSERT_TRUE(memcmp(rates[0],rates[1],sizeof(rates[0]))==0);
    if(cached_neighbors) {
        printf("GPU activity route%u vs%u\n",activity_modes[1],activity_modes[0]);
        ASSERT_TRUE(activity_modes[0]==0u&&activity_modes[1]==1u&&memcmp(rates[0],rates[1],sizeof(rates[0]))==0);
    }
    for(uint32_t k=0;k<4;k++) {ASSERT_EQ(SDL_setenv(names[k],saved[k]?saved[k]:"0",1),0);SDL_free(saved[k]);}
    free(input);free(reference);free(out);free(volume);free(c);free(dormant);free(quiet);PASS();
}
static void test_native_multigrid_temporal_preserves_steps_with_fewer_dispatches(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_MG_BOUNDARY");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY","0",1),0);
    check_native_gas_dispatch_fusion("DC_GAS_MG_TEMPORAL",false,0);
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY",saved?saved:"0",1),0);SDL_free(saved);
}
static void test_native_fused_transport_metadata_preserves_state_with_fewer_dispatches(void) {
    check_native_gas_dispatch_fusion("DC_GAS_FUSED_METADATA",true,0);
}
static void test_native_packed_pressure_rows_preserve_state_with_one_pack_pass(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_HALF_PRESSURE");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_HALF_PRESSURE","0",1),0);
    check_native_gas_dispatch_fusion("DC_GAS_PACKED_PRESSURE",false,1);
    ASSERT_EQ(SDL_setenv("DC_GAS_HALF_PRESSURE",saved?saved:"0",1),0);SDL_free(saved);
}
static void test_native_compressed_pressure_rows_keep_stock_and_reference_flow(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_MG_BOUNDARY");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY","0",1),0);
    check_native_gas_dispatch_fusion("DC_GAS_HALF_PRESSURE",false,1);
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY",saved?saved:"0",1),0);SDL_free(saved);
}
static void test_native_empty_pressure_tiles_are_skipped_without_changing_state(void) {
    check_native_gas_dispatch_fusion("DC_GAS_SKIP_EMPTY_PRESSURE",false,1);
}
static void test_native_group_rate_reduction_preserves_field_cfl_and_clock(void) {
    check_native_gas_dispatch_fusion("DC_GAS_GROUP_RATE",false,UINT32_MAX);
}
static void test_native_first_axis_reads_owner_and_preserves_mapped_and_dormant_stock(void) {
    check_native_gas_dispatch_fusion("DC_GAS_DIRECT_FIRST_AXIS",true,UINT32_MAX);
}
static void test_native_hybrid_axis_fusion_preserves_state_with_fewer_dispatches(void) {
    char err[256]={0};
    const char *value=getenv("DC_GAS_HYBRID_LINES");char *saved=value?SDL_strdup(value):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_HYBRID_LINES","1",1),0);
    check_native_gas_dispatch_fusion("DC_GAS_FUSED_HYBRID",true,0);
    ASSERT_EQ(SDL_setenv("DC_GAS_HYBRID_LINES",saved?saved:"0",1),0);SDL_free(saved);
}
static void test_native_compute_dependencies_preserve_state_without_transfer_scopes(void) {
    check_native_gas_dispatch_fusion("DC_GAS_COMPUTE_BARRIERS",true,UINT32_MAX);
}
static void test_native_axis_specialization_uses_distinct_kernels_with_identical_state(void) {
    char err[256]={0};const char *names[]={"DC_GAS_HYBRID_LINES","DC_GAS_FUSED_HYBRID"};char *saved[2];
    for(uint32_t i=0;i<2;i++) {const char *value=getenv(names[i]);saved[i]=value?SDL_strdup(value):NULL;ASSERT_EQ(SDL_setenv(names[i],"1",1),0);}
    check_native_gas_dispatch_fusion("DC_GAS_FIXED_AXIS",true,UINT32_MAX);
    for(uint32_t i=0;i<2;i++) {ASSERT_EQ(SDL_setenv(names[i],saved[i]?saved[i]:"0",1),0);SDL_free(saved[i]);}
}
static void test_native_publication_keeps_quiet_mapped_and_dormant_stock(void) {
    check_native_gas_dispatch_fusion("DC_GAS_ROTATE_PUBLISH",true,UINT32_MAX);
}
static void test_native_cached_activity_neighbors_preserve_pressure_wake_and_stock(void) {
    check_native_gas_dispatch_fusion("DC_GAS_CACHED_NEIGHBORS",true,UINT32_MAX);
}
static void test_native_lean_metadata_preserves_state_with_one_preparation_pass(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_FUSED_METADATA");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_FUSED_METADATA","1",1),0);
    check_native_gas_dispatch_fusion("DC_GAS_LEAN_METADATA",true,1u);
    ASSERT_EQ(SDL_setenv("DC_GAS_FUSED_METADATA",saved?saved:"0",1),0);SDL_free(saved);
}
static void check_native_multigrid_reference_flow(uint32_t dispatch_budget,bool partial_fixture) {
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
    if(partial_fixture) for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<64;x++) {
        bool wall=(y>=16&&y<=28&&((x>=12&&x<=13)||(x>=19&&x<=20)))||
            (y>=27&&y<=28&&x>=12&&x<=20)||(x>=45&&x<=55&&y>=40&&y<=42);
        if(wall) {input[y*64+x]=(fr_gas_state_t){0};volume[y*64+x]=0;}
    }
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
        if(volume[i]==0) {
            ASSERT_TRUE(out[i].energy==0&&out[i].momentum[0]==0&&out[i].momentum[1]==0);
            for(uint32_t k=0;k<5;k++) ASSERT_TRUE(out[i].species[k]==0&&reference[i].species[k]==0);
            for(uint32_t k=0;k<3;k++) ASSERT_TRUE(out[i].pigment[k]==0&&reference[i].pigment[k]==0);
            continue;
        }
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
    ASSERT_TRUE(new_work<dispatch_budget&&new_work<baseline_work);
    for(uint32_t k=0;k<3;k++) {ASSERT_EQ(SDL_setenv(names[k],saved[k]?saved[k]:"0",1),0);SDL_free(saved[k]);}
    free(input);free(reference);free(out);free(volume);free(c);PASS();
}
static void test_native_multigrid_bounds_work_and_preserves_reference_flow(void) {
    check_native_multigrid_reference_flow(100u,false);
}
static void test_native_partial_coarse_cells_preserve_walls_with_short_smoothing(void) {
    char err[256]={0};const char *old=getenv("DC_GAS_MG_BOUNDARY");char *saved=old?SDL_strdup(old):NULL;
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY","1",1),0);check_native_multigrid_reference_flow(80u,true);
    ASSERT_EQ(SDL_setenv("DC_GAS_MG_BOUNDARY",saved?saved:"0",1),0);SDL_free(saved);
}
#endif
