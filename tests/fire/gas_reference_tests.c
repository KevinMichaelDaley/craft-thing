#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ferrum/fire/gas.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
/* Dimensionless reference coefficients, not a selected production sound speed. */
static fr_gas_model_t model(void) {
    fr_gas_model_t m={.kinetic_scale=1};
    double molecular[5]={32,28,44,18,0};
    for(uint32_t i=0;i<5;i++) {m.gas_constant[i]=molecular[i]?28./molecular[i]:0.;m.heat_capacity_cv[i]=molecular[i]?2.5*m.gas_constant[i]:1.;}
    return m;
}
static fr_gas_state_t air(fr_gas_model_t m,double mass,double temperature,double velocity) {
    fr_gas_state_t s={0};double oxygen=.21*32./(.21*32.+.79*28.);
    s.species[FR_GAS_OXYGEN]=mass*oxygen;s.species[FR_GAS_NITROGEN]=mass*(1.-oxygen);
    for(uint32_t i=0;i<5;i++) s.energy+=s.species[i]*m.heat_capacity_cv[i]*temperature;
    s.momentum[0]=mass*velocity;s.energy+=.5*m.kinetic_scale*mass*velocity*velocity;return s;
}
static void test_sealed_heating_stores_energy_and_raises_pressure_without_losing_oxygen(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,293.15,0),out;fr_gas_thermo_t a,b;
    ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&a,err,sizeof(err)));ASSERT_TRUE(fabs(a.oxygen_mole_fraction-.21)<1e-12);
    double oxygen=s.species[FR_GAS_OXYGEN],before=s.energy;s.energy*=350./293.15;
    ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&b,err,sizeof(err)));
    ASSERT_TRUE(fabs(b.temperature_K-350)<1e-10&&fabs(b.pressure/a.pressure-350./293.15)<1e-12);
    double volume=1,dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,&s,&volume,1,1,1,.4,&dt,err,sizeof(err)));
    for(uint32_t i=0;i<20;i++) {ASSERT_TRUE(fr_gas_reference_step(&m,&s,&volume,1,1,1,dt,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));s=out;}
    ASSERT_EQ(s.species[FR_GAS_OXYGEN],oxygen);ASSERT_TRUE(fabs(s.energy-before*350./293.15)<1e-10);PASS();
}
static void test_reversible_compression_and_expansion_close_mass_energy_and_boundary_work(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,293.15,0),compressed,expanded;double work=0,returned=0;
    ASSERT_TRUE(fr_gas_reference_compress(&m,&s,1,.5,&compressed,&work,err,sizeof(err)));
    fr_gas_thermo_t a,b;ASSERT_TRUE(fr_gas_thermodynamics(&m,&s,1,&a,err,sizeof(err))&&fr_gas_thermodynamics(&m,&compressed,.5,&b,err,sizeof(err)));
    ASSERT_TRUE(fabs(b.temperature_K/a.temperature_K-pow(2.,.4))<1e-10);
    ASSERT_TRUE(fabs(b.pressure/a.pressure-pow(2.,1.4))<1e-10);ASSERT_TRUE(work>0&&fabs(compressed.energy-s.energy-work)<1e-10);
    ASSERT_TRUE(fr_gas_reference_compress(&m,&compressed,.5,1,&expanded,&returned,err,sizeof(err)));
    ASSERT_TRUE(fabs(expanded.energy-s.energy)<1e-10&&fabs(work+returned)<1e-10);
    ASSERT_EQ(memcmp(s.species,expanded.species,sizeof(s.species)),0);PASS();
}
static void test_finite_colored_smoke_dilutes_oxygen_without_consuming_it(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t target=air(m,1,293.15,0),source={0},left,mixed;
    source.species[FR_GAS_EXHAUST]=2;source.energy=2*m.heat_capacity_cv[FR_GAS_EXHAUST]*330;source.pigment[0]=2;
    double oxygen=target.species[FR_GAS_OXYGEN],energy=target.energy+source.energy;
    ASSERT_TRUE(fr_gas_admit(&m,&source,&target,.25,&left,&mixed,err,sizeof(err)));
    ASSERT_EQ(left.species[FR_GAS_EXHAUST],1.5);ASSERT_EQ(mixed.species[FR_GAS_EXHAUST],.5);
    ASSERT_EQ(mixed.species[FR_GAS_OXYGEN],oxygen);ASSERT_TRUE(fabs(left.energy+mixed.energy-energy)<1e-10);
    fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,&mixed,1,&t,err,sizeof(err)));ASSERT_TRUE(t.oxygen_mole_fraction<.21);
    source=left;source.pigment[0]=0;source.pigment[2]=1.5;
    ASSERT_TRUE(fr_gas_admit(&m,&source,&mixed,1,&left,&target,err,sizeof(err)));
    ASSERT_EQ(target.pigment[0]/target.species[FR_GAS_EXHAUST],.25);ASSERT_EQ(target.pigment[2]/target.species[FR_GAS_EXHAUST],.75);
    ASSERT_EQ(left.energy,0.);for(uint32_t i=0;i<5;i++) ASSERT_EQ(left.species[i],0.);PASS();
}
static void test_stationary_pressure_balanced_contacts_do_not_diffuse_pigment_at_sound_speed(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t input[8],out[8];double volume[8];
    for(uint32_t i=0;i<8;i++) {input[i]=air(m,i<4?1:2,i<4?300:150,0);volume[i]=1;}
    double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,input,volume,8,1,1,.4,&dt,err,sizeof(err)));
    ASSERT_TRUE(fr_gas_reference_step(&m,input,volume,8,1,1,dt,FR_GAS_FLUX_HLLC,out,err,sizeof(err)));
    for(uint32_t i=0;i<8;i++) {
        ASSERT_TRUE(fabs(out[i].energy-input[i].energy)<1e-10);
        ASSERT_TRUE(fabs(out[i].species[0]-input[i].species[0])<1e-12&&fabs(out[i].momentum[0])<1e-12);
    }
    PASS();
}
static void test_closed_pressure_release_conserves_species_energy_and_pigment_for_both_solvers(void) {
    char err[256]={0};fr_gas_model_t m=model();
    for(uint32_t solver=0;solver<2;solver++) {
        fr_gas_state_t cells[32],out[32];double volume[32],before[5]={0},energy=0,pigment=0;
        for(uint32_t i=0;i<32;i++) {
            cells[i]=air(m,i<16?1:.125,i<16?300:240,0);volume[i]=1;
            cells[i].species[FR_GAS_EXHAUST]=.01;cells[i].energy+=.01*m.heat_capacity_cv[FR_GAS_EXHAUST]*300;
            cells[i].pigment[0]=i<16?.01:0;cells[i].pigment[2]=i<16?0:.01;
            for(uint32_t k=0;k<5;k++) before[k]+=cells[i].species[k];
            energy+=cells[i].energy;pigment+=cells[i].pigment[0];
        }
        for(uint32_t step=0;step<100;step++) {
            double dt=0;ASSERT_TRUE(fr_gas_reference_max_dt(&m,cells,volume,32,1,1,.4,&dt,err,sizeof(err)));
            ASSERT_TRUE(fr_gas_reference_step(&m,cells,volume,32,1,1,dt,solver,out,err,sizeof(err)));memcpy(cells,out,sizeof(cells));
        }
        double after[5]={0},heat=0,color=0;bool moved=false;
        for(uint32_t i=0;i<32;i++) {
            fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,cells+i,1,&t,err,sizeof(err))&&t.temperature_K>0);
            for(uint32_t k=0;k<5;k++) after[k]+=cells[i].species[k];
            heat+=cells[i].energy;color+=cells[i].pigment[0];moved|=fabs(cells[i].momentum[0])>1e-6;
        }
        for(uint32_t k=0;k<5;k++) ASSERT_TRUE(fabs(after[k]-before[k])<1e-10);
        ASSERT_TRUE(moved&&fabs(heat-energy)<1e-8&&fabs(color-pigment)<1e-10);
    }
    PASS();
}
static void test_cfl_violation_and_invalid_volume_or_admission_are_transactional(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s=air(m,1,300,20),out,saved,other;double volume=1,dt=0;
    memset(&out,0x5a,sizeof(out));saved=out;other=out;
    ASSERT_TRUE(fr_gas_reference_max_dt(&m,&s,&volume,1,1,1,.4,&dt,err,sizeof(err))&&dt>0);
    ASSERT_TRUE(!fr_gas_reference_step(&m,&s,&volume,1,1,1,dt*2,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));
    ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);
    ASSERT_TRUE(!fr_gas_admit(&m,&s,&s,1.01,&out,&other,err,sizeof(err)));ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);
    fr_gas_thermo_t t={.mass=91},copy=t;
    ASSERT_TRUE(!fr_gas_thermodynamics(&m,&s,0,&t,err,sizeof(err)));ASSERT_EQ(memcmp(&t,&copy,sizeof(t)),0);
    ASSERT_TRUE(fr_gas_reference_step(&m,&s,&volume,1,1,1,0,FR_GAS_FLUX_HLLC,&out,err,sizeof(err)));ASSERT_EQ(memcmp(&out,&s,sizeof(s)),0);PASS();
}
int main(void) {
    RUN(test_sealed_heating_stores_energy_and_raises_pressure_without_losing_oxygen);
    RUN(test_reversible_compression_and_expansion_close_mass_energy_and_boundary_work);
    RUN(test_finite_colored_smoke_dilutes_oxygen_without_consuming_it);
    RUN(test_stationary_pressure_balanced_contacts_do_not_diffuse_pigment_at_sound_speed);
    RUN(test_closed_pressure_release_conserves_species_energy_and_pigment_for_both_solvers);
    RUN(test_cfl_violation_and_invalid_volume_or_admission_are_transactional);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
