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
    fr_reactive_profile_t invalid=jelly;invalid.softening_K=NAN;
    ASSERT_TRUE(!fr_bulk_rheology_mobility(&invalid,293.f,20.f,1.f,&result));
    ASSERT_EQ(result,7.f);PASS();
}
static void test_quadrature_mass_reconciles_after_flux_and_resample(void) {
    fr_bulk_rheology_quadrature_t before[4]={{0}},left[4]={{0}},right[4]={{0}};
    ASSERT_TRUE(fr_bulk_rheology_quadrature(40000u,1.3f,4u,before));
    ASSERT_TRUE(fr_bulk_rheology_quadrature(25000u,1.3f,3u,left));
    ASSERT_TRUE(fr_bulk_rheology_quadrature(15000u,1.3f,4u,right));
    uint32_t before_volume=0,left_volume=0,right_volume=0;
    float before_mass=0.f,after_mass=0.f;
    for(uint32_t i=0;i<4u;i++) {
        before_volume+=before[i].volume_fp;before_mass+=before[i].mass;
        right_volume+=right[i].volume_fp;after_mass+=right[i].mass;
        if(i<3u) {left_volume+=left[i].volume_fp;after_mass+=left[i].mass;}
    }
    ASSERT_INT_EQ(before_volume,40000u);
    ASSERT_INT_EQ(left_volume+right_volume,before_volume);
    ASSERT_TRUE(fabsf(after_mass-before_mass)<.00001f);PASS();
}
static void test_quadrature_rejects_invalid_volume_transactionally(void) {
    fr_bulk_rheology_quadrature_t sample[4]={{.volume_fp=77u,.mass=7.f}};
    ASSERT_TRUE(!fr_bulk_rheology_quadrature(65537u,1.3f,4u,sample));
    ASSERT_INT_EQ(sample[0].volume_fp,77u);
    ASSERT_TRUE(!fr_bulk_rheology_quadrature(100u,1.3f,5u,sample));
    ASSERT_INT_EQ(sample[0].volume_fp,77u);PASS();
}
static void test_empty_phase_resample_clears_all_sample_mass(void) {
    fr_bulk_rheology_quadrature_t sample[4]={{.volume_fp=77u,.mass=7.f},
        {.volume_fp=77u,.mass=7.f},{.volume_fp=77u,.mass=7.f},
        {.volume_fp=77u,.mass=7.f}};
    ASSERT_TRUE(fr_bulk_rheology_quadrature(0u,1.3f,4u,sample));
    for(uint32_t i=0;i<4u;i++) {
        ASSERT_INT_EQ(sample[i].volume_fp,0u);ASSERT_EQ(sample[i].mass,0.f);
    }
    PASS();
}
static void test_symmetric_deformation_distinguishes_shear_from_rotation(void) {
    const float shear[4]={0.f,10.f,0.f,0.f};
    const float rotation[4]={0.f,-10.f,10.f,0.f};
    float shear_rate=-1.f,rotation_rate=-1.f;
    ASSERT_TRUE(fr_bulk_rheology_shear_rate(shear,&shear_rate));
    ASSERT_TRUE(fr_bulk_rheology_shear_rate(rotation,&rotation_rate));
    ASSERT_EQ(shear_rate,10.f);
    ASSERT_EQ(rotation_rate,0.f);
    PASS();
}
int main(void) {
    RUN(test_small_load_does_not_yield_cold_sludge);
    RUN(test_larger_load_yields_and_hot_sludge_softens);
    RUN(test_shear_thinning_increases_motion_at_fixed_stress);
    RUN(test_invalid_samples_are_transactional);
    RUN(test_quadrature_mass_reconciles_after_flux_and_resample);
    RUN(test_quadrature_rejects_invalid_volume_transactionally);
    RUN(test_empty_phase_resample_clears_all_sample_mass);
    RUN(test_symmetric_deformation_distinguishes_shear_from_rotation);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail>0?1:0;
}
