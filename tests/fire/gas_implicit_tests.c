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
static void test_implicit_acoustic_mode_matches_independent_backward_euler_at_large_acoustic_cfl(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s[64]={0},out[64];double v[64],before=0;
    const double pi=acos(-1.),temperature=300.,epsilon=.0001,seconds=1.;
    for(uint32_t i=0;i<64;i++) {
        v[i]=1;s[i].species[0]=.21;s[i].species[1]=.79;
        s[i].energy=2.5*temperature*(1.+epsilon*cos(pi*(i+.5)/64.));before+=s[i].energy;
    }
    ASSERT_TRUE(fr_gas_implicit_reference_step(&m,s,v,64,1,seconds,out,err,sizeof(err)));
    double amplitude=0,after=0;double lambda=4.*pow(sin(pi/128.),2),expected=temperature*epsilon/(1.+1.4*temperature*seconds*seconds*lambda);
    for(uint32_t i=0;i<64;i++) {
        fr_gas_thermo_t t;ASSERT_TRUE(fr_gas_thermodynamics(&m,out+i,1,&t,err,sizeof(err)));
        amplitude+=2.*(t.pressure-temperature)*cos(pi*(i+.5)/64.)/64.;after+=out[i].energy;
        ASSERT_EQ(memcmp(s[i].species,out[i].species,sizeof(s[i].species)),0);
    }
    printf("Implicit acoustic amplitude %.12g independent %.12g energy error %.12g\n",amplitude,expected,after-before);
    ASSERT_TRUE(fabs(amplitude-expected)<expected*.002&&fabs(after-before)<1e-7);PASS();
}
static void test_implicit_stationary_colored_contacts_and_walls_preserve_exact_inventory(void) {
    char err[256]={0};fr_gas_model_t m=model();fr_gas_state_t s[16]={0},out[16];double v[16];
    for(uint32_t i=0;i<16;i++) {
        v[i]=i==7?0:1;if(!v[i]) continue;
        double mass=i<7?1:2;s[i].species[2]=mass;s[i].energy=750;
        s[i].pigment[i<7?0:2]=mass;
    }
    ASSERT_TRUE(fr_gas_implicit_reference_step(&m,s,v,16,1,1,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(s,out,sizeof(s)),0);
    memset(out,0x5a,sizeof(out));fr_gas_state_t saved[16];memcpy(saved,out,sizeof(out));
    ASSERT_TRUE(!fr_gas_implicit_reference_step(&m,s,v,16,1,-1,out,err,sizeof(err)));
    ASSERT_EQ(memcmp(saved,out,sizeof(out)),0);PASS();
}
int main(void) {
    RUN(test_implicit_acoustic_mode_matches_independent_backward_euler_at_large_acoustic_cfl);
    RUN(test_implicit_stationary_colored_contacts_and_walls_preserve_exact_inventory);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
