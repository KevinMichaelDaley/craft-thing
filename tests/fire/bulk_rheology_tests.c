#include <stdio.h>
#include <math.h>

#include "ferrum/fire/bulk_rheology.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static const fr_reactive_profile_t jelly={
    .phase=FR_REACTIVE_LIQUID,.yield_stress=12.f,.consistency=4.f,
    .shear_exponent=.5f,.softening_K=360.f,.hot_yield_multiplier=.1f};

static void test_small_load_does_not_yield_cold_sludge(void) {
    float mobility=-1.f;
    ASSERT_TRUE(fr_bulk_rheology_mobility(&jelly,293.f,8.f,1.f,&mobility));
    ASSERT_EQ(mobility,0.f);PASS();
}
static void test_larger_load_yields_and_hot_sludge_softens(void) {
    float cold=0.f,hot=0.f;
    ASSERT_TRUE(fr_bulk_rheology_mobility(&jelly,293.f,16.f,1.f,&cold));
    ASSERT_TRUE(fr_bulk_rheology_mobility(&jelly,390.f,16.f,1.f,&hot));
    ASSERT_TRUE(cold>0.f&&hot>cold&&hot<=1.f);PASS();
}
static void test_shear_thinning_increases_motion_at_fixed_stress(void) {
    float slow=0.f,fast=0.f;
    ASSERT_TRUE(fr_bulk_rheology_mobility(&jelly,293.f,24.f,.1f,&slow));
    ASSERT_TRUE(fr_bulk_rheology_mobility(&jelly,293.f,24.f,10.f,&fast));
    ASSERT_TRUE(fast>slow);PASS();
}
static void test_invalid_samples_are_transactional(void) {
    float result=7.f;
    ASSERT_TRUE(!fr_bulk_rheology_mobility(&jelly,293.f,NAN,1.f,&result));
    ASSERT_EQ(result,7.f);PASS();
}
int main(void) {
    RUN(test_small_load_does_not_yield_cold_sludge);
    RUN(test_larger_load_yields_and_hot_sludge_softens);
    RUN(test_shear_thinning_increases_motion_at_fixed_stress);
    RUN(test_invalid_samples_are_transactional);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail>0?1:0;
}
