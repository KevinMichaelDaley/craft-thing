#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/combustion.h"
#include "ferrum/fire/material_profile.h"
#include "dungeoncraft/combustion_parameters.h"
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
static void test_fractional_reaction_survives_checkpoint_and_split(void) {
    char err[256]={0};fr_reactive_record_t record={.owner=fr_reactive_cell_owner((dc_chunk_coord_t){0,0},0)},restored;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,47,&record.state,err,sizeof(err)));
    record.state.flags|=FR_REACTIVE_FLAMING;fr_reactive_state_t next;fr_combustion_products_t products;
    ASSERT_TRUE(fr_combustion_reference(&record.state,65536,.23f,1,.001f,&next,&products,err,sizeof(err)));
    ASSERT_TRUE(next.reaction_fraction>0&&next.reaction_fraction<1);record.state=next;
    uint8_t bytes[512];size_t written=0;uint32_t count=0,required=0;uint64_t id=0;
    ASSERT_TRUE(fr_reactive_snapshot_encode(&record,1,48,bytes,sizeof(bytes),&written,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,written,&restored,1,&count,&id,&required,err,sizeof(err)));
    ASSERT_EQ(count,1);ASSERT_EQ(restored.state.reaction_fraction,next.reaction_fraction);
    fr_reactive_state_t left,right;
    ASSERT_TRUE(fr_reactive_state_split(&next,32768,49,50,&left,&right,err,sizeof(err)));
    ASSERT_TRUE(fabsf(left.reaction_fraction+right.reaction_fraction-next.reaction_fraction)<1e-7f);PASS();
}
static void test_retained_char_remains_finite_combustible_fuel(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_CHAR,65536,0,900,53,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_combustion_reference(&s,65536,.23f,1,.1f,&out,&p,err,sizeof(err)));
    ASSERT_TRUE(out.char_mass_fp<s.char_mass_fp&&p.ash_fp>0);
    ASSERT_EQ(s.char_mass_fp-out.char_mass_fp,p.fuel_fp);
    ASSERT_EQ((uint64_t)p.fuel_fp+p.oxygen_fp,(uint64_t)p.ash_fp+p.soot_fp+p.exhaust_fp);
    ASSERT_TRUE(fr_reactive_state_valid(&out,err,sizeof(err)));PASS();
}
static fr_gas_model_t gas_model(void) {
    return (fr_gas_model_t){.gas_constant={.25984,.29680,.18892,.46152,0},
        .heat_capacity_cv={.65816,.74320,.65508,1.40248,.71},.kinetic_scale=1./512.};
}
static fr_gas_state_t airborne_fuel(fr_gas_model_t model,double temperature,bool fuel,bool oxygen) {
    fr_gas_state_t s={0};s.species[FR_GAS_OXYGEN]=oxygen?.00028:0;
    s.species[FR_GAS_NITROGEN]=.00072;s.species[fuel?FR_GAS_SOOT:FR_GAS_EXHAUST]=.0002;
    for(uint32_t i=0;i<FR_GAS_SPECIES;i++) s.energy+=s.species[i]*model.heat_capacity_cv[i]*temperature;
    return s;
}
static void test_airborne_reaction_requires_transported_fuel_heat_and_oxygen(void) {
    char err[256]={0};fr_gas_model_t model=gas_model();
    for(uint32_t control=0;control<4;control++) {
        fr_gas_state_t before=airborne_fuel(model,control==2?300:1100,control!=1,control!=3),after;
        double reacted=-1;
        ASSERT_TRUE(fr_combustion_gas_reference(&model,&before,1,.02,&after,&reacted,err,sizeof(err)));
        if(control==0) {
            ASSERT_TRUE(reacted>0&&reacted<before.species[FR_GAS_SOOT]);
            ASSERT_TRUE(after.species[FR_GAS_OXYGEN]<before.species[FR_GAS_OXYGEN]);
            ASSERT_TRUE(after.species[FR_GAS_EXHAUST]>0);
            double mass_before=0,mass_after=0;
            for(uint32_t i=0;i<FR_GAS_SPECIES;i++) {mass_before+=before.species[i];mass_after+=after.species[i];}
            ASSERT_TRUE(fabs(mass_before-mass_after)<1e-14);
            ASSERT_TRUE(fabs(after.energy-before.energy-reacted*FR_COMBUSTION_AIRBORNE_ENERGY)<1e-12);
        } else {ASSERT_EQ(reacted,0);ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);}
    }
    PASS();
}
static void test_surface_reserves_airborne_chemical_heat_instead_of_creating_it(void) {
    char err[256]={0};fr_reactive_state_t s,out;fr_combustion_products_t p;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,65536,0,900,59,&s,err,sizeof(err)));
    s.flags|=FR_REACTIVE_FLAMING;
    ASSERT_TRUE(fr_combustion_reference(&s,65536,.23f,1,1,&out,&p,err,sizeof(err)));
    ASSERT_TRUE(p.soot_fp>0&&p.reserved_airborne_heat>0);
    ASSERT_TRUE(fabs(p.released_heat+p.reserved_airborne_heat-
        p.fuel_fp/65536.*fr_reactive_profile(DC_MATERIAL_WOOD)->primary_heat)<1e-10);PASS();
}
int main(void) {
    RUN(test_finite_oxygen_and_products);
    RUN(test_starved_wet_and_buried_do_not_burn);
    RUN(test_ignition_hazard_survives_equal_time_scheduling);
    RUN(test_energy_ledger_and_invalid_transaction);
    RUN(test_competing_parcels_cannot_overdraw_oxygen);
    RUN(test_short_frames_retain_fractional_burn_progress);
    RUN(test_fractional_reaction_survives_checkpoint_and_split);
    RUN(test_retained_char_remains_finite_combustible_fuel);
    RUN(test_airborne_reaction_requires_transported_fuel_heat_and_oxygen);
    RUN(test_surface_reserves_airborne_chemical_heat_instead_of_creating_it);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
