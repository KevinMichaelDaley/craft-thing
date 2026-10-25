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
static fr_gas_model_t model(void) {
    return (fr_gas_model_t){.gas_constant={1,1,1,1,0},.heat_capacity_cv={2.5,2.5,2.5,2.5,1},.kinetic_scale=1};
}
static fr_gas_state_t air(double u,double v) {
    return (fr_gas_state_t){.species={.2,.7,.1,0,0},.energy=750.+.5*(u*u+v*v),.momentum={u,v}};
}
static void test_cfl_four_matches_independent_geometric_packet_and_closed_budgets(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s[32],out[32];double v[32];
    for(uint32_t i=0;i<32;i++) {v[i]=1;s[i]=air(1,0);}
    s[4].pigment[0]=.1;s[6].pigment[2]=.1;
    ASSERT_TRUE(fr_gas_transport_reference_step(&m,s,v,32,1,4,out,err,sizeof(err)));
    double energy=0,red=0,blue=0,mass=0;
    for(uint32_t i=0;i<32;i++) {
        double expected=i<4?0:(i==31?.1*pow(.8,27):.02*pow(.8,i-4));
        ASSERT_TRUE(fabs(out[i].pigment[0]-expected)<1e-12);
        fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,1,&t,err,sizeof(err)));
        ASSERT_TRUE(t.temperature_K>0&&out[i].pigment[0]>=0&&out[i].pigment[2]>=0);
        energy+=out[i].energy;red+=out[i].pigment[0];blue+=out[i].pigment[2];mass+=t.mass;
    }
    ASSERT_TRUE(fabs(mass-32.)<1e-10&&fabs(energy-32.*750.5)<1e-8&&fabs(red-.1)<1e-12&&fabs(blue-.1)<1e-12);PASS();
}
static void test_two_dimensional_large_cfl_keeps_walls_species_energy_and_pigment(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s[64]={0},out[64];double v[64],before[11]={0},after[11]={0};
    for(uint32_t i=0;i<64;i++) {
        v[i]=i%8==4?0:1;if(!v[i]) continue;s[i]=air(.5,-.25);s[i].pigment[i%8<4?0:2]=.1;
        double words[11];memcpy(words,s+i,sizeof(words));for(uint32_t k=0;k<11;k++) before[k]+=words[k];
    }
    ASSERT_TRUE(fr_gas_transport_reference_step(&m,s,v,8,8,2,out,err,sizeof(err)));
    for(uint32_t i=0;i<64;i++) {
        fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,v[i],&t,err,sizeof(err)));
        if(i%8<4) ASSERT_EQ(out[i].pigment[2],0.);
        if(i%8>4) ASSERT_EQ(out[i].pigment[0],0.);
        double words[11];memcpy(words,out+i,sizeof(words));for(uint32_t k=0;k<11;k++) after[k]+=words[k];
    }
    for(uint32_t k=0;k<11;k++) ASSERT_TRUE(fabs(after[k]-before[k])<1e-8);
    PASS();
}
static void test_invalid_and_zero_time_are_transactional(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s[2]={air(1,0),air(1,0)},out[2],saved[2];double v[2]={1,1};
    memset(out,0x5a,sizeof(out));memcpy(saved,out,sizeof(out));
    ASSERT_TRUE(!fr_gas_transport_reference_step(&m,s,v,2,1,NAN,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(saved,out,sizeof(out)),0);
    ASSERT_TRUE(fr_gas_transport_reference_step(&m,s,v,2,1,0,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(s,out,sizeof(out)),0);PASS();
}
int main(void) {
    RUN(test_cfl_four_matches_independent_geometric_packet_and_closed_budgets);
    RUN(test_two_dimensional_large_cfl_keeps_walls_species_energy_and_pigment);
    RUN(test_invalid_and_zero_time_are_transactional);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
