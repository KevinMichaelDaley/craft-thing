#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal.h"
#include "dungeoncraft/thermal_surface.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_sample_uses_actual_composition_and_keeps_wet_wood_capacity(void) {
    char err[256]={0};fr_reactive_state_t state;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,13763,330,1,&state,err,sizeof(err)));
    fr_thermal_sample_t sample;ASSERT_TRUE(fr_thermal_sample_from_state(&state,&sample,err,sizeof(err)));
    double expected=(39322.0*1.5+13763.0*4.18)/65536.0;
    ASSERT_TRUE(fabs(sample.capacity-expected)<1e-6);ASSERT_TRUE(fabs(sample.energy/sample.capacity-330)<1e-4);
    state.dry_mass_fp*=2;fr_thermal_sample_t changed;
    ASSERT_TRUE(fr_thermal_sample_from_state(&state,&changed,err,sizeof(err)));
    ASSERT_EQ(changed.capacity,sample.capacity);ASSERT_EQ(changed.energy,sample.energy);PASS();
}
static void test_pair_exchange_matches_face_conductance_and_conserves_energy(void) {
    char err[256]={0};fr_thermal_sample_t stone={2.16f*350.f,2.16f,1.5f},wood={.9f*293.15f,.9f,.12f};float flux=0;
    ASSERT_TRUE(fr_thermal_pair_flux(stone,wood,1.f/60.f,&flux,err,sizeof(err)));
    double expected=(2.0*1.5*.12/(1.5+.12))*(350.0-293.15)/60.0;
    ASSERT_TRUE(fabs(flux-expected)<2e-6);
    double before=(double)stone.energy+wood.energy;stone.energy-=flux;wood.energy+=flux;
    ASSERT_TRUE(fabs((double)stone.energy+wood.energy-before)<5e-5);
    float reverse=0;ASSERT_TRUE(fr_thermal_pair_flux(wood,stone,1.f/60.f,&reverse,err,sizeof(err)));ASSERT_TRUE(reverse<0);PASS();
}
static void test_four_face_update_cannot_overshoot_even_for_large_time_steps(void) {
    char err[256]={0};fr_thermal_sample_t center={.9f*350.f,.9f,.12f},cold={2.16f*293.15f,2.16f,1.5f};float flux=0;
    ASSERT_TRUE(fr_thermal_pair_flux(center,cold,100,&flux,err,sizeof(err)));
    float after=(center.energy-4.f*flux)/center.capacity;
    ASSERT_TRUE(after>=293.15f-1e-4f&&after<=350.f);ASSERT_TRUE((cold.energy+flux)/cold.capacity<=350.f);PASS();
}
static void test_finite_probe_energy_decreases_without_temperature_clamping_or_creation(void) {
    char err[256]={0};fr_thermal_probe_t probe={.capacity=FR_THERMAL_PROBE_CAPACITY,
        .excess_energy=FR_THERMAL_PROBE_CAPACITY*(FR_THERMAL_PROBE_K-FR_THERMAL_AMBIENT_K)};
    fr_thermal_sample_t wood={.energy=.9f*293.15f,.capacity=.9f,.conductivity=.12f};
    double before=probe.excess_energy+wood.energy;
    for(uint32_t i=0;i<1000;i++) {
        fr_thermal_probe_t next_probe;fr_thermal_sample_t next_wood;
        ASSERT_TRUE(fr_thermal_probe_exchange(probe,wood,1.f/60.f,&next_probe,&next_wood,err,sizeof(err)));
        probe=next_probe;wood=next_wood;
        ASSERT_TRUE(wood.energy/wood.capacity<=FR_THERMAL_PROBE_K+1e-4&&probe.excess_energy>=0);
    }
    ASSERT_TRUE(wood.energy/wood.capacity>330);ASSERT_TRUE(fabs(probe.excess_energy+wood.energy-before)<before*1e-4);PASS();
}
static void test_wetting_conserves_water_and_enthalpy_and_preserves_history(void) {
    char err[256]={0};fr_reactive_state_t wood,water,out_wood,out_water;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,1,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WATER,65536,0,293.15f,2,&water,err,sizeof(err)));
    wood.ignition_threshold=.7f;wood.ignition_integral=.4f;
    ASSERT_TRUE(fr_thermal_absorb_pair(&wood,&water,1.f/60.f,&out_wood,&out_water,err,sizeof(err)));
    ASSERT_EQ(out_wood.bound_water_fp,16u);ASSERT_EQ(out_water.inert_mass_fp,65520u);
    ASSERT_EQ(out_wood.bound_water_fp+out_water.inert_mass_fp,water.inert_mass_fp);
    ASSERT_TRUE(fabs((double)out_wood.thermal_energy+out_water.thermal_energy-wood.thermal_energy-water.thermal_energy)<1e-4);
    ASSERT_EQ(out_wood.ignition_threshold,wood.ignition_threshold);ASSERT_EQ(out_wood.ignition_integral,wood.ignition_integral);
    ASSERT_EQ(out_wood.parcel_id_low,wood.parcel_id_low);ASSERT_EQ(out_water.parcel_id_low,water.parcel_id_low);
    fr_thermal_sample_t sample;ASSERT_TRUE(fr_thermal_sample_from_state(&out_wood,&sample,err,sizeof(err)));
    ASSERT_TRUE(sample.energy/sample.capacity<350.f);PASS();
}
static void test_pause_and_invalid_operations_leave_state_unchanged(void) {
    char err[256]={0};fr_reactive_state_t wood,water,a,b;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,1,&wood,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WATER,65536,0,293.15f,2,&water,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_absorb_pair(&wood,&water,0,&a,&b,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&a,&wood,sizeof(a))==0&&memcmp(&b,&water,sizeof(b))==0);
    fr_reactive_state_t saved_a=a,saved_b=b;
    ASSERT_TRUE(!fr_thermal_absorb_pair(&wood,&water,-1,&a,&b,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&a,&saved_a,sizeof(a))==0&&memcmp(&b,&saved_b,sizeof(b))==0);
    fr_thermal_sample_t sample={1,1,1};float flux=99;
    ASSERT_TRUE(fr_thermal_pair_flux(sample,sample,0,&flux,err,sizeof(err)));ASSERT_EQ(flux,0.f);
    sample.capacity=0;flux=99;ASSERT_TRUE(!fr_thermal_pair_flux(sample,sample,1,&flux,err,sizeof(err)));ASSERT_EQ(flux,99.f);PASS();
}
static void test_finite_radiance_heats_the_boundary_without_requiring_a_hot_bulk(void) {
    char err[256]={0};float coefficient=1e-12f;
    float surface=fr_thermal_surface_K(293.15f,620,.0024f,.001f,coefficient,coefficient*1400.f*1400.f*1400.f*1400.f);
    ASSERT_TRUE(surface>600&&surface<1400);
    float residual=coefficient*surface*surface*surface*surface+.0034f*surface-
        (coefficient*1400.f*1400.f*1400.f*1400.f+.0024f*293.15f+.001f*620);
    ASSERT_TRUE(fabsf(residual)<.01f);PASS();
}
static void test_equilibrium_boundary_cannot_invent_a_hot_surface(void) {
    char err[256]={0};float coefficient=1e-12f;
    ASSERT_TRUE(fabsf(fr_thermal_surface_K(300,300,.0024f,.001f,coefficient,coefficient*300.f*300.f*300.f*300.f)-300)<.1f);
    ASSERT_TRUE(fabsf(fr_thermal_surface_K(300,300,.0024f,.001f,0,0)-300)<.1f);PASS();
}
int main(void) {
    RUN(test_finite_radiance_heats_the_boundary_without_requiring_a_hot_bulk);
    RUN(test_equilibrium_boundary_cannot_invent_a_hot_surface);
    RUN(test_sample_uses_actual_composition_and_keeps_wet_wood_capacity);
    RUN(test_pair_exchange_matches_face_conductance_and_conserves_energy);
    RUN(test_four_face_update_cannot_overshoot_even_for_large_time_steps);
    RUN(test_finite_probe_energy_decreases_without_temperature_clamping_or_creation);
    RUN(test_wetting_conserves_water_and_enthalpy_and_preserves_history);
    RUN(test_pause_and_invalid_operations_leave_state_unchanged);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
