#define main gas_cell_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "ferrum/fire/gas_gpu.h"
#undef RUN
#define RUN(fn) do {const char *filter=getenv("DC_GAS_TEST_FILTER");if(!filter||strstr(#fn,filter)) {printf("RUN %s\n",#fn);fn();}} while(0)

static fr_gas_model_t reference_model(void) {
    fr_gas_model_t m={.kinetic_scale=1};double molecular[5]={32,28,44,18,0};
    for(uint32_t i=0;i<5;i++) {
        double r=molecular[i]?28./molecular[i]:0;
        m.gas_constant[i]=(float)r;m.heat_capacity_cv[i]=(float)(molecular[i]?2.5*r:1.);
    }
    return m;
}
static fr_gas_state_t reference_air(fr_gas_model_t m,double mass,double temperature,double vx) {
    fr_gas_state_t s={0};double oxygen=(.21/m.gas_constant[0])/(.21/m.gas_constant[0]+.79/m.gas_constant[1]);
    s.species[0]=mass*oxygen;s.species[1]=mass-s.species[0];
    for(uint32_t i=0;i<5;i++) s.energy+=s.species[i]*m.heat_capacity_cv[i]*temperature;
    s.momentum[0]=mass*vx;s.energy+=.5*m.kinetic_scale*mass*vx*vx;return s;
}
#include "gas_checkpoint_cases.h"
#include "gas_geometry_cases.h"
#include "gas_partial_void_cases.h"
#include "gas_roundoff_cases.h"
static void test_native_resting_ambient_and_pause_preserve_exact_stock_and_clock(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
    fr_gas_state_t *before=malloc(4096*sizeof(*before)),*after=malloc(4096*sizeof(*after));ASSERT_TRUE(before&&after);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,before,4096,err,sizeof(err)));
    fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,before,1,&t,err,sizeof(err)));
    ASSERT_TRUE(fabs(t.oxygen_mole_fraction-.21)<1e-6&&fabs(t.temperature_K-300)<1e-4);
    ASSERT_TRUE(fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));
    ASSERT_EQ(memcmp(before,after,4096*sizeof(*before)),0);
    fr_gas_gpu_clock_t clock,paused;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));ASSERT_EQ(clock.physical_steps,1u);
    ASSERT_TRUE(clock.substeps>1&&fabs(clock.advanced_seconds-1./60.)<1e-8);
    ASSERT_TRUE(!fr_gas_gpu_step(g,NAN,err,sizeof(err))&&fr_gas_gpu_step(g,0,err,sizeof(err))&&present(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_clock(g,&paused)&&memcmp(&clock,&paused,sizeof(clock))==0);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));ASSERT_EQ(memcmp(before,after,4096*sizeof(*before)),0);
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err))&&fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,after,4096,err,sizeof(err)));ASSERT_EQ(memcmp(before,after,4096*sizeof(*before)),0);
    free(before);free(after);free(c);dc_gpu_destroy(g);PASS();
}
static void colored_transport(uint32_t policy) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();fr_gas_state_t *input=calloc(4096,sizeof(*input)),*expected=calloc(4096,sizeof(*expected)),*out=calloc(4096,sizeof(*out));
    double *volumes=malloc(4096*sizeof(*volumes));ASSERT_TRUE(input&&expected&&out&&volumes);
    for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<64;x++) {
        uint32_t i=y*64+x;bool wall=x==32&&(y<20||y>=44);volumes[i]=wall?0:1;
        if(wall) {c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};continue;}
        input[i]=reference_air(m,1,300,0);
        if(x==0&&y>=10&&y<16) input[i]=reference_air(m,1,300,2);
        if(x>=24&&x<32&&y>=26&&y<38) {
            input[i].species[FR_GAS_EXHAUST]=.1;input[i].energy+=.1*m.heat_capacity_cv[FR_GAS_EXHAUST]*330;
            input[i].pigment[x<28?0:2]=.1;input[i].momentum[0]=1.1*20;
            input[i].energy+=.5*m.kinetic_scale*1.1*20*20;
        }
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,policy,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    double before_mass[5]={0},before_energy=0,before_color=0;
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) before_mass[k]+=input[i].species[k];
        before_energy+=input[i].energy;before_color+=input[i].pigment[0];
    }
    float interval=.001f;
    if(policy==FR_GAS_PRESSURE_HLLC) {
        ASSERT_TRUE(fr_gas_reference_step(&m,input,volumes,64,64,1,interval,FR_GAS_FLUX_HLLC,expected,err,sizeof(err)));
    } else {
        ASSERT_TRUE(fr_gas_implicit_reference_step(&m,input,volumes,64,64,interval,expected,err,sizeof(err)));
    }
    ASSERT_TRUE(fr_gas_gpu_step(g,interval,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(out[i].species[k]-expected[i].species[k])<2e-6);
        ASSERT_TRUE(fabs(out[i].energy-expected[i].energy)<.002);
        for(uint32_t k=0;k<2;k++) ASSERT_TRUE(fabs(out[i].momentum[k]-expected[i].momentum[k])<2e-5);
    }
    for(uint32_t step=1;step<30;step++) ASSERT_TRUE(fr_gas_gpu_step(g,interval,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));double mass[5]={0},energy=0,color=0;bool mixed=false;
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) mass[k]+=out[i].species[k];
        energy+=out[i].energy;color+=out[i].pigment[0];mixed|=out[i].pigment[0]>1e-4&&out[i].pigment[2]>1e-4;
        if(volumes[i]==0) {for(uint32_t k=0;k<5;k++) ASSERT_EQ(out[i].species[k],0.);}
    }
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(mass[k]-before_mass[k])<.0002);
    printf("Native gas budgets: energy error%.9g red error%.9g\n",energy-before_energy,color-before_color);
    ASSERT_TRUE(fabs(energy-before_energy)<.02&&fabs(color-before_color)<2e-5&&mixed);
    ASSERT_TRUE(present(g,err,sizeof(err)));
    uint32_t *pixels=malloc(480u*270u*4u);ASSERT_TRUE(pixels&&dc_gpu_readback(g,pixels,480u*270u,err,sizeof(err)));
    mkdir("build/screenshots",0777);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(surface);
    ASSERT_EQ(SDL_SaveBMP(surface,"build/screenshots/gas_reference_transport.bmp"),0);SDL_FreeSurface(surface);free(pixels);
    free(input);free(expected);free(out);free(volumes);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_colored_transport_matches_reference_and_retains_closed_budgets(void) {colored_transport(FR_GAS_PRESSURE_HLLC);}
static void test_native_implicit_colored_transport_matches_reference_and_retains_closed_budgets(void) {colored_transport(FR_GAS_PRESSURE_IMPLICIT);}
static void test_native_buoyancy_matches_reference_and_accounts_for_external_work(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err)));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));
    double *volumes=malloc(4096*sizeof(*volumes)),*work=malloc(4096*sizeof(*work));ASSERT_TRUE(input&&out&&volumes&&work);
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(m,.5,600,0);volumes[i]=1;}
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
    float seconds=.0001f;
    ASSERT_TRUE(fr_gas_gpu_step(g,seconds,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_read_work_chunk(g,0,work,4096,err,sizeof(err)));
    fr_gas_state_t ambient=reference_air(m,1,300,0);double specific=0,total_work=0,energy_change=0;
    for(uint32_t k=0;k<5;k++) specific+=ambient.species[k]*m.gas_constant[k]*300;
    for(uint32_t i=0;i<4096;i++) {
        fr_gas_state_t expected;double admitted=0;
        ASSERT_TRUE(fr_gas_reference_buoyancy(&m,input+i,1,specific,900,seconds,&expected,&admitted,err,sizeof(err)));
        ASSERT_TRUE(out[i].momentum[1]<0&&fabs(out[i].momentum[1]-expected.momentum[1])<2e-7);
        ASSERT_TRUE(fabs(out[i].energy-expected.energy)<1e-6&&fabs(work[i]-admitted)<1e-7);
        ASSERT_EQ(memcmp(out[i].species,input[i].species,sizeof(out[i].species)),0);
        energy_change+=out[i].energy-input[i].energy;total_work+=work[i];
    }
    ASSERT_TRUE(fabs(energy_change-total_work)<1e-6);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(out);free(volumes);free(work);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_implicit_acoustics_match_reference_beyond_explicit_acoustic_cfl(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out)),*expected=malloc(4096*sizeof(*expected));
    double *volumes=malloc(4096*sizeof(*volumes));ASSERT_TRUE(input&&out&&expected&&volumes);
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(m,1,300*(1.+.0001*cos(acos(-1.)*((i%64)+.5)/64.)),0);volumes[i]=1;}
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    float seconds=.05f;ASSERT_TRUE(fr_gas_implicit_reference_step(&m,input,volumes,64,64,seconds,expected,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,seconds,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    double energy_error=0;
    for(uint32_t i=0;i<4096;i++) {
        ASSERT_EQ(memcmp(input[i].species,out[i].species,sizeof(out[i].species)),0);
        ASSERT_TRUE(fabs(out[i].energy-expected[i].energy)<.0002);
        ASSERT_TRUE(fabs(out[i].momentum[0]-expected[i].momentum[0])<2e-5&&fabs(out[i].momentum[1]-expected[i].momentum[1])<2e-5);
        energy_error+=out[i].energy-input[i].energy;
    }
    ASSERT_TRUE(fabs(energy_error)<.02&&present(g,err,sizeof(err)));
    free(input);free(out);free(expected);free(volumes);free(c);dc_gpu_destroy(g);PASS();
}
static bool seam_snapshot(dc_gpu_t *g,fr_gas_state_t *global,fr_gas_state_t *local,char *err,uint32_t cap) {
    for(uint32_t chunk=0;chunk<2;chunk++) {
        if(!fr_gas_gpu_read_chunk(g,chunk,local,4096,err,cap)) return false;
        for(uint32_t y=0;y<64;y++) memcpy(global+y*128+chunk*64,local+y*64,64*sizeof(*local));
    }
    return true;
}
static void test_native_implicit_sparse_pressure_and_color_cross_chunk_seams_without_stock_loss(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    for(uint32_t slot=0;slot<2;slot++) {c->coord.x=slot;ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,c,err,sizeof(err))&&dc_gpu_set_page(g,slot,0,slot,err,sizeof(err)));}
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    fr_gas_state_t *input=malloc(8192*sizeof(*input)),*out=malloc(8192*sizeof(*out)),*expected=malloc(8192*sizeof(*expected)),*local=malloc(4096*sizeof(*local));
    double *volumes=malloc(8192*sizeof(*volumes));ASSERT_TRUE(input&&out&&expected&&local&&volumes);
    for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<128;x++) {
        uint32_t i=y*128+x;volumes[i]=1;input[i]=reference_air(m,1,300,0);
        if(x>=60&&x<68&&y>=28&&y<36) {
            input[i].species[2]=.1;input[i].energy+=.1*m.heat_capacity_cv[2]*330+.5*m.kinetic_scale*1.1*20*20;
            input[i].momentum[0]=1.1*20;input[i].pigment[x<64?0:2]=.1;
        }
    }
    for(uint32_t slot=0;slot<2;slot++) {
        for(uint32_t y=0;y<64;y++) memcpy(local+y*64,input+y*128+slot*64,64*sizeof(*local));
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,slot,local,volumes,4096,err,sizeof(err)));
    }
    ASSERT_TRUE(seam_snapshot(g,input,local,err,sizeof(err)));
    double before_mass[5]={0},before_energy=0,before_color=0;
    for(uint32_t i=0;i<8192;i++) {for(uint32_t k=0;k<5;k++) before_mass[k]+=input[i].species[k];before_energy+=input[i].energy;before_color+=input[i].pigment[0];}
    float interval=.001f;ASSERT_TRUE(fr_gas_implicit_reference_step(&m,input,volumes,128,64,interval,expected,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,interval,err,sizeof(err))&&seam_snapshot(g,out,local,err,sizeof(err)));
    for(uint32_t i=0;i<8192;i++) {
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(out[i].species[k]-expected[i].species[k])<2e-6);
        ASSERT_TRUE(fabs(out[i].energy-expected[i].energy)<.002);
        for(uint32_t k=0;k<2;k++) ASSERT_TRUE(fabs(out[i].momentum[k]-expected[i].momentum[k])<2e-5);
    }
    for(uint32_t step=1;step<20;step++) ASSERT_TRUE(fr_gas_gpu_step(g,interval,err,sizeof(err)));
    ASSERT_TRUE(seam_snapshot(g,out,local,err,sizeof(err)));double mass[5]={0},energy=0,color=0;
    for(uint32_t i=0;i<8192;i++) {for(uint32_t k=0;k<5;k++) mass[k]+=out[i].species[k];energy+=out[i].energy;color+=out[i].pigment[0];}
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(mass[k]-before_mass[k])<.0002);
    ASSERT_TRUE(fabs(energy-before_energy)<.02&&fabs(color-before_color)<2e-5);
    printf("Native gas seam: red-right%.9g blue-right%.9g energy error%.9g pigment error%.9g\n",
        out[32*128+64].pigment[0],out[32*128+64].pigment[2],energy-before_energy,color-before_color);
    ASSERT_TRUE(out[32*128+64].pigment[0]>1e-5&&out[32*128+64].pigment[2]>1e-5&&present(g,err,sizeof(err)));
    free(input);free(out);free(expected);free(local);free(volumes);free(c);dc_gpu_destroy(g);PASS();
}
static void finite_gate(uint32_t policy) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,policy,err,sizeof(err)));
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*out=malloc(4096*sizeof(*out));double *volumes=malloc(4096*sizeof(*volumes));ASSERT_TRUE(input&&out&&volumes);
    for(uint32_t i=0;i<4096;i++) {
        volumes[i]=1;input[i]=reference_air(m,1,300,0);
        if(i%64<32) {input[i].species[2]=.1;input[i].energy+=.1*m.heat_capacity_cv[2]*330;input[i].pigment[0]=.1;}
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,true,err,sizeof(err))&&fr_gas_gpu_step(g,.005f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_EQ(memcmp(input,out,4096*sizeof(*input)),0);
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,false,err,sizeof(err))&&fr_gas_gpu_step(g,.005f,err,sizeof(err))&&
        fr_gas_gpu_step(g,.005f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    double mass[5]={0},energy=0,color=0;
    for(uint32_t i=0;i<4096;i++) {for(uint32_t k=0;k<5;k++) mass[k]+=out[i].species[k]-input[i].species[k];energy+=out[i].energy-input[i].energy;color+=out[i].pigment[0]-input[i].pigment[0];}
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(mass[k])<.0002);
    ASSERT_TRUE(fabs(energy)<.02&&fabs(color)<2e-5&&out[32*64+32].pigment[0]>1e-5&&present(g,err,sizeof(err)));
    free(input);free(out);free(volumes);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_closed_gate_retains_stock_then_opens_real_hllc_flow(void) {finite_gate(FR_GAS_PRESSURE_HLLC);}
static void test_native_closed_gate_retains_stock_then_opens_real_implicit_flow(void) {finite_gate(FR_GAS_PRESSURE_IMPLICIT);}
static void test_native_oxygen_and_valve_feedback_are_visible_and_do_not_advance_stock(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=malloc(4096*sizeof(*out));double *volumes=malloc(4096*sizeof(*volumes));
    uint32_t *smoke=malloc(480u*270u*4u),*oxygen=malloc(480u*270u*4u),*closed=malloc(480u*270u*4u);ASSERT_TRUE(input&&out&&volumes&&smoke&&oxygen&&closed);
    for(uint32_t i=0;i<4096;i++) {
        volumes[i]=1;if(i%64>=32) input[i]=reference_air(m,1,300,0);
        else {input[i].species[2]=1;input[i].energy=m.heat_capacity_cv[2]*300;input[i].pigment[0]=.8;input[i].pigment[2]=.5;}
    }
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    fr_gas_gpu_clock_t before,after;ASSERT_TRUE(fr_gas_gpu_clock(g,&before));
    ASSERT_TRUE(present(g,err,sizeof(err))&&dc_gpu_readback(g,smoke,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_overlay(g,1)&&present(g,err,sizeof(err))&&dc_gpu_readback(g,oxygen,480u*270u,err,sizeof(err)));
    uint32_t left=oxygen[32u*480u+16u],right=oxygen[32u*480u+48u],changed=0;
    for(uint32_t i=0;i<480u*270u;i++) changed+=smoke[i]!=oxygen[i];
    ASSERT_TRUE(changed>3000u&&(left&255u)>((left>>8u)&255u)&&((right>>16u)&255u)>(right&255u));
    ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,true,err,sizeof(err))&&present(g,err,sizeof(err))&&dc_gpu_readback(g,closed,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(closed[32u*480u+31u]!=oxygen[32u*480u+31u]);
    ASSERT_TRUE(present(g,err,sizeof(err))&&dc_gpu_readback(g,smoke,480u*270u,err,sizeof(err)));
    ASSERT_EQ(memcmp(smoke,closed,480u*270u*4u),0);
    ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&fr_gas_gpu_clock(g,&after));
    ASSERT_EQ(memcmp(input,out,4096*sizeof(*input)),0);ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(closed,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/gas_oxygen_feedback.bmp"),0);SDL_FreeSurface(image);
    free(input);free(out);free(volumes);free(smoke);free(oxygen);free(closed);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_gas_automatically_follows_full_tick_clock_and_present_pause(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(g,1.f/120.f)&&dc_gpu_tick_step(g,err,sizeof(err)));
    fr_gas_gpu_clock_t clock,paused;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));
    ASSERT_EQ(clock.physical_steps,1u);ASSERT_TRUE(fabs(clock.advanced_seconds-1./120.)<1e-8);
    ASSERT_TRUE(dc_gpu_present_chunks_steps(g,2,err,sizeof(err))&&fr_gas_gpu_clock(g,&clock));
    ASSERT_EQ(clock.physical_steps,3u);ASSERT_TRUE(fabs(clock.advanced_seconds-3./120.)<1e-8);
    ASSERT_TRUE(present(g,err,sizeof(err))&&fr_gas_gpu_clock(g,&paused));ASSERT_EQ(memcmp(&clock,&paused,sizeof(clock)),0);
    free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_sealed_hot_cell_cannot_gain_heat_from_wall_blocked_buoyancy(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    uint32_t hot=20u*64u+20u;c->cells[hot]=(dc_cell_t){0};
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    fr_gas_model_t m=reference_model();ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=malloc(4096*sizeof(*out));double *volumes=calloc(4096,sizeof(*volumes)),*work=malloc(4096*sizeof(*work));ASSERT_TRUE(input&&out&&volumes&&work);
    input[hot]=reference_air(m,.5,600,0);volumes[hot]=1;
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volumes,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,.02f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err))&&fr_gas_gpu_read_work_chunk(g,0,work,4096,err,sizeof(err)));
    printf("Sealed hot cell: energy delta%.9g momentum%.9g external work%.9g\n",out[hot].energy-input[hot].energy,out[hot].momentum[1],work[hot]);
    ASSERT_EQ(memcmp(input,out,4096*sizeof(*input)),0);ASSERT_EQ(work[hot],0.);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(out);free(volumes);free(work);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_derived_cache_rebuilds_after_seed_and_page_remap(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();
    fr_gas_state_t *input=malloc(4096*sizeof(*input)),*first=malloc(4096*sizeof(*first)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(input&&first&&out&&volume);
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(m,1,300,(i%64>=24&&i%64<32)?2:0);volume[i]=1;}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,first,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_EQ(memcmp(first,out,4096*sizeof(*out)),0);
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&dc_gpu_set_page(g,1,0,0,err,sizeof(err))&&fr_gas_gpu_step(g,.001f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_EQ(memcmp(first,out,4096*sizeof(*out)),0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(first);free(out);free(volume);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_implicit_axis_cfl_matches_cold_oracle_in_one_positive_step(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();fr_gas_state_t *input=malloc(4096*sizeof(*input)),*expected=malloc(4096*sizeof(*expected)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(input&&expected&&out&&volume);
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(m,1,300,2);input[i].momentum[1]=1;input[i].energy+=.5;volume[i]=1;}
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_implicit_reference_step(&m,input,volume,64,64,.25,expected,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,.25f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));ASSERT_EQ(clock.substeps,1u);
    double before=0,after=0,mass=0;
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) {ASSERT_TRUE(out[i].species[k]>=0&&fabs(out[i].species[k]-expected[i].species[k])<2e-6);mass+=out[i].species[k];}
        ASSERT_TRUE(fabs(out[i].energy-expected[i].energy)<.005);
        for(uint32_t k=0;k<2;k++) ASSERT_TRUE(fabs(out[i].momentum[k]-expected[i].momentum[k])<.0002);
        before+=input[i].energy;after+=out[i].energy;
    }
    ASSERT_TRUE(fabs(mass-4096.)<.0002&&fabs(after-before)<.02);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(expected);free(out);free(volume);free(c);dc_gpu_destroy(g);PASS();
}
static void sparse_footprints_match_dense(uint32_t policy,float seconds,double velocity,double kinetic_scale) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();fr_gas_state_t *input=malloc(4096*sizeof(*input)),*dense=malloc(4096*sizeof(*dense)),*out=malloc(4096*sizeof(*out));
    double *volume=malloc(4096*sizeof(*volume));ASSERT_TRUE(input&&dense&&out&&volume);
    m.kinetic_scale=kinetic_scale;
    for(uint32_t i=0;i<4096;i++) {
        volume[i]=1;input[i]=reference_air(m,1,300,0);
        if(i%64>=12&&i%64<20&&i/64>=20&&i/64<28) input[i]=reference_air(m,1,300,velocity);
        if(i%64>=44&&i%64<52&&i/64>=44&&i/64<52) input[i]=reference_air(m,.5,600,0);
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,policy,err,sizeof(err)));
    const char *names[5]={"DC_GAS_SPARSE_RHS","DC_GAS_SPARSE_ACTIVITY","DC_GAS_SPARSE_CLASSIFY","DC_GAS_SPARSE_LARGE","DC_GAS_FUSED_APPLY"};char *saved[5]={0};
    for(uint32_t k=0;k<5;k++) {const char *s=getenv(names[k]);saved[k]=s?SDL_strdup(s):NULL;}
    for(uint32_t path=0;path<2;path++) {
        const char *control=getenv("DC_GAS_TEST_SPARSE_CONTROL");
        for(uint32_t k=0;k<5;k++) ASSERT_EQ(SDL_setenv(names[k],path&&(!control||strcmp(control,names[k])==0)?"1":"0",1),0);
        ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_set_gravity(g,0,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,true,err,sizeof(err)));
        for(uint32_t step=0;step<20;step++) {
            if(step==7) ASSERT_TRUE(fr_gas_gpu_set_gravity(g,900,err,sizeof(err)));
            if(step==12) ASSERT_TRUE(fr_gas_gpu_gate(g,31,0,1,64,0,false,err,sizeof(err)));
            ASSERT_TRUE(fr_gas_gpu_step(g,seconds,err,sizeof(err)));
        }
        ASSERT_TRUE(fr_gas_gpu_read_chunk(g,0,path?out:dense,4096,err,sizeof(err)));
    }
    for(uint32_t k=0;k<5;k++) {ASSERT_EQ(SDL_setenv(names[k],saved[k]?saved[k]:"1",1),0);SDL_free(saved[k]);}
    if(memcmp(dense,out,4096*sizeof(*out))) {
        uint32_t printed=0;
        for(uint32_t i=0;i<4096&&printed<8;i++) {
            double a[11],b[11];memcpy(a,dense+i,sizeof(a));memcpy(b,out+i,sizeof(b));
            for(uint32_t k=0;k<11&&printed<8;k++) if(memcmp(a+k,b+k,8)) {printf("Footprint difference cell%u word%u %.17g %.17g delta%.9g\n",i,k,a[k],b[k],b[k]-a[k]);printed++;}
        }
    }
    ASSERT_EQ(memcmp(dense,out,4096*sizeof(*out)),0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(input);free(dense);free(out);free(volume);free(c);dc_gpu_destroy(g);PASS();
}
static void test_native_sparse_footprints_match_dense_after_gravity_and_valve_changes(void) {
    sparse_footprints_match_dense(FR_GAS_PRESSURE_IMPLICIT,.001f,2,1);
}
static void test_native_large_cfl_sparse_footprints_match_dense_after_gravity_and_valve_changes(void) {
    sparse_footprints_match_dense(FR_GAS_PRESSURE_LARGE_CFL,.05f,25,1);
}
static void test_native_prototype_sparse_large_footprints_match_dense(void) {
    sparse_footprints_match_dense(FR_GAS_PRESSURE_LARGE_CFL,.01f,100,1./512.);
}
static void test_native_explicit_sustained_gravity_control(void) {
    sparse_footprints_match_dense(FR_GAS_PRESSURE_HLLC,.05f,25,1);
}
static void test_native_large_cfl_packet_matches_composed_oracle_in_one_step(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_gas_model_t m=reference_model();fr_gas_state_t *input=calloc(4096,sizeof(*input)),*expected=malloc(4096*sizeof(*expected)),*out=malloc(4096*sizeof(*out));
    double *volume=calloc(4096,sizeof(*volume));ASSERT_TRUE(input&&expected&&out&&volume);
    for(uint32_t i=0;i<4096;i++) c->cells[i]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    for(uint32_t x=0;x<64;x++) {
        uint32_t i=20u*64u+x;c->cells[i]=(dc_cell_t){0};volume[i]=1;input[i]=reference_air(m,.9,300,10);
        input[i].species[FR_GAS_EXHAUST]=.1;input[i].energy+=.1*m.heat_capacity_cv[FR_GAS_EXHAUST]*300+5;input[i].momentum[0]=10;
        if(x==4) input[i].pigment[0]=.1;
        if(x==6) input[i].pigment[2]=.1;
    }
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_LARGE_CFL,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,input,4096,err,sizeof(err)));
    float seconds=.4f;ASSERT_TRUE(fr_gas_large_cfl_reference_step(&m,input,volume,64,64,seconds,expected,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,seconds,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    fr_gas_gpu_clock_t clock;ASSERT_TRUE(fr_gas_gpu_clock(g,&clock));ASSERT_EQ(clock.substeps,1u);
    double energy=0,original=0,stock[5]={0},before[5]={0};
    for(uint32_t i=0;i<4096;i++) {
        for(uint32_t k=0;k<5;k++) {ASSERT_TRUE(fabs(out[i].species[k]-expected[i].species[k])<2e-5);stock[k]+=out[i].species[k];before[k]+=input[i].species[k];}
        for(uint32_t k=0;k<3;k++) ASSERT_TRUE(fabs(out[i].pigment[k]-expected[i].pigment[k])<2e-6);
        for(uint32_t k=0;k<2;k++) ASSERT_TRUE(fabs(out[i].momentum[k]-expected[i].momentum[k])<.0002);
        ASSERT_TRUE(fabs(out[i].energy-expected[i].energy)<.01);energy+=out[i].energy;original+=input[i].energy;
    }
    for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(stock[k]-before[k])<.0002);
    ASSERT_TRUE(fabs(energy-original)<.02&&present(g,err,sizeof(err)));
    free(input);free(expected);free(out);free(volume);free(c);dc_gpu_destroy(g);PASS();
}
#include "gas_large_cfl_cases.h"
#include "gas_practical_cases.h"
#ifdef FR_GAS_PUBLICATION_TEST
#include "gas_publication_cases.h"
#include "gas_multigrid_cases.h"
#include "gas_geometry_cache_cases.h"
#include "gas_pressure_noise_cases.h"
#include "gas_compact_line_cases.h"
#endif
static void test_native_subnormal_colored_stock_is_valid_and_retained_at_rest(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *c=calloc(1,sizeof(*c));
    fr_gas_state_t *input=calloc(4096,sizeof(*input)),*out=calloc(4096,sizeof(*out));double *volume=malloc(4096*sizeof(*volume));
    ASSERT_TRUE(c&&input&&out&&volume);fr_gas_model_t m=reference_model();
    for(uint32_t i=0;i<4096;i++) {input[i]=reference_air(m,1,300,0);volume[i]=1;}
    input[20u*64u+20u].species[FR_GAS_EXHAUST]=(float)1e-39;
    input[20u*64u+20u].pigment[0]=(float)9e-40;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&dc_gpu_upload_chunk(g,0,c,err,sizeof(err))&&dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_enable(g,&m,1,300,err,sizeof(err))&&fr_gas_gpu_set_pressure_policy(g,FR_GAS_PRESSURE_IMPLICIT,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_seed_chunk(g,0,input,volume,4096,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_gpu_step(g,1.f/60.f,err,sizeof(err))&&fr_gas_gpu_read_chunk(g,0,out,4096,err,sizeof(err)));
    ASSERT_EQ(out[20u*64u+20u].pigment[0],input[20u*64u+20u].pigment[0]);
    ASSERT_EQ(out[20u*64u+20u].species[FR_GAS_EXHAUST],input[20u*64u+20u].species[FR_GAS_EXHAUST]);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(c);free(input);free(out);free(volume);dc_gpu_destroy(g);PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
#ifdef FR_GAS_PUBLICATION_TEST
    RUN(test_native_publication_uses_one_submission_and_preserves_rejected_state);
    RUN(test_native_tick_waits_its_fence_without_draining_presentation_queue);
    RUN(test_native_temporal_sor_matches_global_and_bounds_dispatches);
    RUN(test_native_multigrid_bounds_work_and_preserves_reference_flow);
    RUN(test_native_multigrid_temporal_preserves_steps_with_fewer_dispatches);
    RUN(test_native_fused_transport_metadata_preserves_state_with_fewer_dispatches);
    RUN(test_native_packed_pressure_rows_preserve_state_with_one_pack_pass);
    RUN(test_native_empty_pressure_tiles_are_skipped_without_changing_state);
    RUN(test_native_partial_coarse_cells_preserve_walls_with_short_smoothing);
    RUN(test_native_compressed_pressure_rows_keep_stock_and_reference_flow);
    RUN(test_native_group_rate_reduction_preserves_field_cfl_and_clock);
    RUN(test_native_first_axis_reads_owner_and_preserves_mapped_and_dormant_stock);
    RUN(test_native_hybrid_axis_fusion_preserves_state_with_fewer_dispatches);
    RUN(test_native_compute_dependencies_preserve_state_without_transfer_scopes);
    RUN(test_native_fence_trace_excludes_slow_log_output);
    RUN(test_native_axis_specialization_uses_distinct_kernels_with_identical_state);
    RUN(test_native_publication_keeps_quiet_mapped_and_dormant_stock);
    RUN(test_native_cached_activity_neighbors_preserve_pressure_wake_and_stock);
    RUN(test_native_lean_metadata_preserves_state_with_one_preparation_pass);
    RUN(test_native_large_hybrid_workgroups_preserve_state_and_clock);
    RUN(test_native_zero_energy_transfers_preserve_state_stock_and_clock);
    RUN(test_native_local_geometry_activity_keeps_exact_edited_and_quiet_owners);
    RUN(test_native_sparse_bank_owners_preserve_checkpoints_dormant_cells_and_policy_changes);
    RUN(test_native_reused_line_rate_preserves_stiff_packet_and_owners);
    RUN(test_native_device_work_memory_preserves_state_initialization_and_stock);
    RUN(test_native_device_state_memory_preserves_checkpoint_initialization_owners_and_clock);
    RUN(test_native_local_geometry_repairs_keep_quiet_owners_and_avoid_full_cache_rebuilds);
    RUN(test_native_pressure_noise_admission_defers_weak_rows_with_stable_inventory);
    RUN(test_native_compact_line_routes_preserve_explicit_implicit_and_quiet_packets);
    RUN(test_native_fused_geometry_keeps_edited_stock_and_removes_completion_wait);
#endif
    RUN(test_native_resting_ambient_and_pause_preserve_exact_stock_and_clock);
    RUN(test_native_colored_transport_matches_reference_and_retains_closed_budgets);
    RUN(test_native_buoyancy_matches_reference_and_accounts_for_external_work);
    RUN(test_native_implicit_acoustics_match_reference_beyond_explicit_acoustic_cfl);
    RUN(test_native_implicit_colored_transport_matches_reference_and_retains_closed_budgets);
    RUN(test_native_implicit_sparse_pressure_and_color_cross_chunk_seams_without_stock_loss);
    RUN(test_native_closed_gate_retains_stock_then_opens_real_hllc_flow);
    RUN(test_native_closed_gate_retains_stock_then_opens_real_implicit_flow);
    RUN(test_native_oxygen_and_valve_feedback_are_visible_and_do_not_advance_stock);
    RUN(test_native_gas_automatically_follows_full_tick_clock_and_present_pause);
    RUN(test_native_sealed_hot_cell_cannot_gain_heat_from_wall_blocked_buoyancy);
    RUN(test_native_derived_cache_rebuilds_after_seed_and_page_remap);
    RUN(test_native_implicit_axis_cfl_matches_cold_oracle_in_one_positive_step);
    RUN(test_native_sparse_footprints_match_dense_after_gravity_and_valve_changes);
    RUN(test_native_large_cfl_packet_matches_composed_oracle_in_one_step);
    RUN(test_native_large_cfl_crosses_chunk_and_subgroup_seams_with_mixed_color);
    RUN(test_native_large_cfl_closed_valve_matches_two_independent_chambers);
    RUN(test_native_large_cfl_two_dimensional_concave_wall_matches_oracle);
    RUN(test_native_large_cfl_balanced_counterflow_has_no_artificial_compression);
    RUN(test_native_subnormal_colored_stock_is_valid_and_retained_at_rest);
    RUN(test_native_large_cfl_subnormal_packet_keeps_one_step_and_closed_stock);
    RUN(test_native_receding_gate_matches_real_reflecting_wall);
    RUN(test_native_prototype_sparse_large_footprints_match_dense);
    if(getenv("DC_GAS_EXTREME_STRESS")) {
        RUN(test_native_large_cfl_sparse_footprints_match_dense_after_gravity_and_valve_changes);
        RUN(test_native_explicit_sustained_gravity_control);
    } else printf("Extreme unresolved-gravity diagnostic cases require DC_GAS_EXTREME_STRESS=1; practical admission rejects that regime.\n");
    RUN(test_native_moving_density_contact_keeps_pressure_and_velocity);
    RUN(test_native_practical_policy_rejects_unresolved_gravity_without_advancing);
    RUN(test_native_quiet_air_sleeps_but_motion_heat_and_pressure_still_wake);
    RUN(test_native_practical_safe_line_matches_explicit_flux_inside_stiff_interval);
    RUN(test_native_cold_audit_reduces_stock_and_does_not_advance_physics);
    RUN(test_native_practical_checkerboard_pressure_relaxes_conservatively);
    RUN(test_native_practical_offset_wall_does_not_drive_sealed_neighbor);
    RUN(test_native_practical_variable_intervals_stay_finite_without_retry_stalls);
    RUN(test_native_checkpoint_preserves_compensation_geometry_valves_and_ownership);
    RUN(test_native_new_streamed_slot_initializes_once_without_refilling_existing_or_dormant_stock);
    RUN(test_native_geometry_carving_displacement_and_trapped_pockets_keep_finite_stock);
    RUN(test_native_partial_void_contraction_displaces_mixture_without_cut_cell_pressure_spikes);
    RUN(test_native_valve_only_refresh_preserves_restored_physical_void);
    RUN(test_native_massless_underflow_energy_reconciles_without_erasing_tiny_colored_gas);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0||g_pass==0;
}
