#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "dungeoncraft/material_density.h"
#include "ferrum/fire/material_profile.h"

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static fr_reactive_exposure_t hot(void) {
    return (fr_reactive_exposure_t){.temperature_K=1000,.oxygen_mass_fraction=.21f};
}
static void test_registered_profiles_keep_old_ids_and_density_defaults(void) {
    ASSERT_EQ(DC_MATERIAL_WOOD,7u);ASSERT_EQ(DC_MATERIAL_LEAF_DEBRIS,10u);
    ASSERT_EQ(fr_reactive_profile_count(),DC_MATERIAL_COUNT);
    char err[128];
    for(uint32_t id=0;id<DC_MATERIAL_COUNT;id++) {
        const fr_reactive_profile_t *p=fr_reactive_profile(id);
        ASSERT_TRUE(p);ASSERT_EQ(p->material,id);
        ASSERT_TRUE(fr_reactive_profile_valid(p,err,sizeof(err)));
        ASSERT_TRUE(fr_reactive_material_name(id));
    }
    ASSERT_EQ(fr_reactive_profile(DC_MATERIAL_STONE)->density,(float)DC_DENSITY_STONE);
    ASSERT_EQ(fr_reactive_profile(DC_MATERIAL_WOOD)->density,(float)DC_DENSITY_WOOD);
    ASSERT_EQ(fr_reactive_profile(DC_MATERIAL_LEAVES)->density,(float)DC_DENSITY_LEAVES);
    ASSERT_TRUE(!fr_reactive_profile(UINT32_MAX));PASS();
}
static void test_body_namespace_is_mapped_without_numeric_reinterpretation(void) {
    const fr_reactive_profile_t *wood=fr_reactive_profile_for_body(2u);
    ASSERT_TRUE(wood);ASSERT_EQ(wood->material,DC_MATERIAL_WOOD);
    const fr_reactive_profile_t *mortar=fr_reactive_profile_for_body(3u);
    ASSERT_TRUE(mortar);ASSERT_EQ(mortar->material,DC_MATERIAL_MORTAR_DEBRIS);
    ASSERT_TRUE(!fr_reactive_profile_for_body(0u));
    ASSERT_TRUE(!fr_reactive_profile_for_body(UINT32_MAX));PASS();
}
static void test_ash_and_sludge_have_distinct_phase_and_fuel_properties(void) {
    const fr_reactive_profile_t *ash=fr_reactive_profile(DC_MATERIAL_ASH);
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *jelly=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    const fr_reactive_profile_t *greek=fr_reactive_profile(DC_MATERIAL_GREEK_FIRE);
    ASSERT_TRUE(ash&&oil&&jelly&&greek);
    ASSERT_EQ(ash->phase,FR_REACTIVE_GRANULAR);ASSERT_EQ(ash->fuel_fraction,0.f);
    ASSERT_EQ(oil->phase,FR_REACTIVE_LIQUID);ASSERT_TRUE(oil->yield_stress>0);
    ASSERT_TRUE(oil->shear_exponent>0&&oil->shear_exponent<1);
    ASSERT_TRUE(jelly->yield_stress>oil->yield_stress);
    ASSERT_TRUE((greek->flags&FR_REACTIVE_BOUND_OXIDIZER)!=0);
    ASSERT_TRUE((greek->flags&FR_REACTIVE_WATER_DOUSEABLE)==0);
    ASSERT_TRUE(greek->surface_burn_per_second>oil->surface_burn_per_second);
    PASS();
}
static void test_wetness_and_oxidation_modify_substrate_ignition(void) {
    const fr_reactive_profile_t *wood=fr_reactive_profile(DC_MATERIAL_WOOD);
    ASSERT_TRUE(wood);char err[128];fr_reactive_ignition_rates_t dry,wet,oxidized;
    fr_reactive_exposure_t e=hot();
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&dry,err,sizeof(err)));
    e.saturation=1;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&wet,err,sizeof(err)));
    ASSERT_TRUE(wet.substrate<dry.substrate*.01f);
    e.saturation=0;e.substrate_oxidation=1;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&oxidized,err,sizeof(err)));
    ASSERT_TRUE(oxidized.substrate>dry.substrate);
    PASS();
}
static void test_only_the_coating_burns_on_hot_inert_stone(void) {
    const fr_reactive_profile_t *stone=fr_reactive_profile(DC_MATERIAL_STONE);
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    ASSERT_TRUE(stone&&oil);char err[128];fr_reactive_ignition_rates_t rates;
    fr_reactive_exposure_t e=hot();e.coating_coverage=.5f;
    ASSERT_TRUE(fr_reactive_ignition_rates(stone,oil,&e,&rates,err,sizeof(err)));
    ASSERT_EQ(rates.substrate,0.f);ASSERT_TRUE(rates.coating>0);
    e.coating_coverage=0;
    ASSERT_TRUE(fr_reactive_ignition_rates(stone,oil,&e,&rates,err,sizeof(err)));
    ASSERT_EQ(rates.coating,0.f);PASS();
}
static void test_oil_increases_exposed_wood_hazard_and_shock_is_finite_heat(void) {
    const fr_reactive_profile_t *wood=fr_reactive_profile(DC_MATERIAL_WOOD);
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    ASSERT_TRUE(wood&&oil);char err[128];fr_reactive_ignition_rates_t bare,oiled,shocked;
    fr_reactive_exposure_t e=hot();
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&bare,err,sizeof(err)));
    e.coating_coverage=.5f;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,oil,&e,&oiled,err,sizeof(err)));
    ASSERT_TRUE(oiled.substrate+oiled.coating>bare.substrate);
    e=hot();e.temperature_K=293.15f;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&bare,err,sizeof(err)));
    ASSERT_EQ(bare.substrate,0.f);e.shock_energy_per_mass=600;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&shocked,err,sizeof(err)));
    ASSERT_TRUE(shocked.substrate>0);
    ASSERT_TRUE(fr_reactive_profile(DC_MATERIAL_WATER)->electrical_conductivity>wood->electrical_conductivity);
    ASSERT_EQ(fr_reactive_profile(DC_MATERIAL_WATER)->fuel_fraction,0.f);PASS();
}
static void test_oxygen_and_bound_oxidizer_are_different_supplies(void) {
    const fr_reactive_profile_t *wood=fr_reactive_profile(DC_MATERIAL_WOOD);
    const fr_reactive_profile_t *greek=fr_reactive_profile(DC_MATERIAL_GREEK_FIRE);
    ASSERT_TRUE(wood&&greek);char err[128];fr_reactive_ignition_rates_t rates;
    fr_reactive_exposure_t e=hot();e.oxygen_mass_fraction=0;
    ASSERT_TRUE(fr_reactive_ignition_rates(wood,NULL,&e,&rates,err,sizeof(err)));
    ASSERT_EQ(rates.substrate,0.f);
    e.saturation=1;e.bound_oxidizer_per_fuel=.43f;
    ASSERT_TRUE(fr_reactive_ignition_rates(greek,NULL,&e,&rates,err,sizeof(err)));
    ASSERT_TRUE(rates.substrate>0);
    e.bound_oxidizer_per_fuel=0;
    ASSERT_TRUE(fr_reactive_ignition_rates(greek,NULL,&e,&rates,err,sizeof(err)));
    ASSERT_EQ(rates.substrate,0.f);PASS();
}
static void test_invalid_profiles_and_exposures_do_not_publish_output(void) {
    const fr_reactive_profile_t *wood=fr_reactive_profile(DC_MATERIAL_WOOD);
    ASSERT_TRUE(wood);char err[128];fr_reactive_profile_t p=*wood;
    p.density=NAN;ASSERT_TRUE(!fr_reactive_profile_valid(&p,err,sizeof(err)));
    p=*wood;p.primary_heat=p.chemical_energy+1;
    ASSERT_TRUE(!fr_reactive_profile_valid(&p,err,sizeof(err)));
    p=*wood;p.fuel_yields[0]=2;
    ASSERT_TRUE(!fr_reactive_profile_valid(&p,err,sizeof(err)));
    p=*wood;p.flags|=0x80000000u;
    ASSERT_TRUE(!fr_reactive_profile_valid(&p,err,sizeof(err)));
    fr_reactive_ignition_rates_t rates={12,34},saved=rates;
    fr_reactive_exposure_t e=hot();e.saturation=-1;
    ASSERT_TRUE(!fr_reactive_ignition_rates(wood,NULL,&e,&rates,err,sizeof(err)));
    ASSERT_EQ(memcmp(&rates,&saved,sizeof(rates)),0);PASS();
}
static void test_mixing_oil_and_jelly_does_not_create_chemical_energy(void) {
    const fr_reactive_profile_t *oil=fr_reactive_profile(DC_MATERIAL_OIL);
    const fr_reactive_profile_t *jelly=fr_reactive_profile(DC_MATERIAL_RED_SLIME_JELLY);
    const fr_reactive_profile_t *greek=fr_reactive_profile(DC_MATERIAL_GREEK_FIRE);
    ASSERT_TRUE(oil&&jelly&&greek);
    float input=.5f*oil->fuel_fraction*oil->chemical_energy+
                .5f*jelly->fuel_fraction*jelly->chemical_energy;
    float output=greek->fuel_fraction*greek->chemical_energy;
    printf("mixture energy input %.6f output %.6f\n",input,output);
    ASSERT_TRUE(output<=input+.01f);PASS();
}
int main(void) {
    _Static_assert(sizeof(fr_reactive_profile_t)==128,"GPU profile stride changed");
    _Static_assert(offsetof(fr_reactive_profile_t,fuel_yields)==56,"GPU yield offset changed");
    RUN(test_registered_profiles_keep_old_ids_and_density_defaults);
    RUN(test_body_namespace_is_mapped_without_numeric_reinterpretation);
    RUN(test_ash_and_sludge_have_distinct_phase_and_fuel_properties);
    RUN(test_wetness_and_oxidation_modify_substrate_ignition);
    RUN(test_only_the_coating_burns_on_hot_inert_stone);
    RUN(test_oil_increases_exposed_wood_hazard_and_shock_is_finite_heat);
    RUN(test_oxygen_and_bound_oxidizer_are_different_supplies);
    RUN(test_invalid_profiles_and_exposures_do_not_publish_output);
    RUN(test_mixing_oil_and_jelly_does_not_create_chemical_energy);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
