#define main gas_cell_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "ferrum/fire/gas_gpu.h"

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
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_native_resting_ambient_and_pause_preserve_exact_stock_and_clock);
    RUN(test_native_colored_transport_matches_reference_and_retains_closed_budgets);
    RUN(test_native_buoyancy_matches_reference_and_accounts_for_external_work);
    RUN(test_native_implicit_acoustics_match_reference_beyond_explicit_acoustic_cfl);
    RUN(test_native_implicit_colored_transport_matches_reference_and_retains_closed_budgets);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
