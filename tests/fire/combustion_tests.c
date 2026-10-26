#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/combustion.h"
#include "ferrum/fire/material_profile.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_finite_oxygen_and_products(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,0,800,17,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_combustion_reference(&s,64,.23f,1,1,&out,&p,err,sizeof(err)));
    ASSERT_TRUE(p.fuel_fp>0&&p.fuel_fp<65536&&p.oxygen_fp<=64);
    ASSERT_EQ(s.fuel_mass_fp-out.fuel_mass_fp,p.fuel_fp);
    ASSERT_EQ((uint64_t)p.fuel_fp+p.oxygen_fp,
        (uint64_t)out.char_mass_fp+p.ash_fp+p.soot_fp+p.exhaust_fp);
    ASSERT_TRUE(p.released_heat>0&&isfinite(p.exported_energy));PASS();
}
static void test_starved_wet_and_buried_do_not_burn(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,32768,800,19,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_combustion_reference(&s,65536,.23f,1,1,&out,&p,err,sizeof(err)));
    ASSERT_EQ(p.fuel_fp,0);
    s.bound_water_fp=0;
    ASSERT_TRUE(fr_combustion_reference(&s,0,0,1,1,&out,&p,err,sizeof(err)));
    ASSERT_EQ(p.fuel_fp,0);
    ASSERT_TRUE(fr_combustion_reference(&s,65536,.23f,0,1,&out,&p,err,sizeof(err)));
    ASSERT_EQ(p.fuel_fp,0);PASS();
}
static void test_ignition_hazard_survives_equal_time_scheduling(void) {
    char err[256]={0};fr_reactive_state_t a,b,tmp;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,0,800,29,&a,err,sizeof(err)));
    b=a;
    ASSERT_TRUE(fr_combustion_reference(&a,65536,.23f,1,.125f,&tmp,&p,err,sizeof(err)));a=tmp;
    for(int i=0;i<8;i++) {
        ASSERT_TRUE(fr_combustion_reference(&b,65536,.23f,1,.015625f,&tmp,&p,err,sizeof(err)));b=tmp;
    }
    ASSERT_EQ(a.ignition_threshold,b.ignition_threshold);
    ASSERT_TRUE(fabsf(a.ignition_integral-b.ignition_integral)<1e-6f);
    ASSERT_EQ(a.flags&FR_REACTIVE_FLAMING,b.flags&FR_REACTIVE_FLAMING);PASS();
}
static void test_energy_ledger_and_invalid_transaction(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,0,800,31,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_combustion_reference(&s,65536,.23f,1,1,&out,&p,err,sizeof(err)));
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&out)+p.exported_energy-
        fr_reactive_state_energy(&s)-p.released_heat)<1e-6);
    fr_reactive_state_t saved=out;fr_combustion_products_t saved_products=p;
    ASSERT_TRUE(!fr_combustion_reference(&s,65536,.23f,1,NAN,&out,&p,err,sizeof(err)));
    ASSERT_EQ(memcmp(&saved,&out,sizeof(out)),0);
    ASSERT_EQ(memcmp(&saved_products,&p,sizeof(p)),0);PASS();
}
static void test_competing_parcels_cannot_overdraw_oxygen(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,0,800,37,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;uint32_t remaining=80,used=0;
    for(int i=0;i<8;i++) {
        ASSERT_TRUE(fr_combustion_reference(&s,remaining,.23f,1,1,&out,&p,err,sizeof(err)));
        ASSERT_TRUE(p.oxygen_fp<=remaining);remaining-=p.oxygen_fp;used+=p.oxygen_fp;
    }
    ASSERT_EQ(used+remaining,80);ASSERT_TRUE(remaining<2);PASS();
}
static void test_short_frames_retain_fractional_burn_progress(void) {
    char err[256]={0};fr_reactive_state_t a,b,tmp;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,43,&a,err,sizeof(err)));
    a.flags|=FR_REACTIVE_FLAMING;b=a;
    ASSERT_TRUE(fr_combustion_reference(&a,65536,.23f,1,1,&tmp,&p,err,sizeof(err)));a=tmp;
    for(int i=0;i<240;i++) {
        ASSERT_TRUE(fr_combustion_reference(&b,65536,.23f,1,1.f/240.f,&tmp,&p,err,sizeof(err)));b=tmp;
    }
    ASSERT_TRUE(b.fuel_mass_fp<39322);
    ASSERT_TRUE(abs((int)a.fuel_mass_fp-(int)b.fuel_mass_fp)<=1);
    ASSERT_TRUE(abs((int)a.char_mass_fp-(int)b.char_mass_fp)<=1);PASS();
}
int main(void) {
    RUN(test_finite_oxygen_and_products);
    RUN(test_starved_wet_and_buried_do_not_burn);
    RUN(test_ignition_hazard_survives_equal_time_scheduling);
    RUN(test_energy_ledger_and_invalid_transaction);
    RUN(test_competing_parcels_cannot_overdraw_oxygen);
    RUN(test_short_frames_retain_fractional_burn_progress);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
