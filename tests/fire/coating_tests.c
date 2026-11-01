#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/coating.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static fr_coating_film_t rigid_film(void) {
    fr_coating_film_t film={.host={FR_COATING_RIGID,42u,0u,7u,12u,8u}};
    return film;
}

static void test_wetting_debits_mobile_oil_and_preserves_heat(void) {
    fr_bulk_cell_t bulk={.volume_fp={10000u,30000u,0u},.capacity_fp=65536u};
    fr_bulk_cell_t after={0};fr_coating_film_t film=rigid_film(),wetted={0};
    double heat=0.;uint32_t moved=0u;
    ASSERT_TRUE(fr_coating_contact_wet(&bulk,60.,&film,FR_BULK_OIL,12000u,
        &after,&heat,&wetted,&moved));
    ASSERT_EQ(moved,12000u);ASSERT_EQ(after.volume_fp[FR_BULK_OIL],18000u);
    ASSERT_EQ(wetted.oil_fp,12000u);ASSERT_EQ(heat,36.);
    ASSERT_EQ(wetted.excess_heat,24.);
    ASSERT_EQ((uint64_t)after.volume_fp[FR_BULK_OIL]+wetted.oil_fp,
        (uint64_t)bulk.volume_fp[FR_BULK_OIL]+film.oil_fp);
    ASSERT_EQ(memcmp(&wetted.host,&film.host,sizeof(film.host)),0);PASS();
}

static void test_retained_slime_does_not_wet_and_existing_film_accumulates(void) {
    fr_bulk_cell_t bulk={.volume_fp={0u,0u,8000u},
        .retained_fp={0u,0u,20000u},.capacity_fp=65536u};
    fr_bulk_cell_t after={0};fr_coating_film_t film=rigid_film(),wetted={0};
    film.slime_fp=4000u;film.excess_heat=4.;double heat=0.;uint32_t moved=0u;
    ASSERT_TRUE(fr_coating_contact_wet(&bulk,16.,&film,FR_BULK_SLIME,20000u,
        &after,&heat,&wetted,&moved));
    ASSERT_EQ(moved,8000u);ASSERT_EQ(after.volume_fp[FR_BULK_SLIME],0u);
    ASSERT_EQ(after.retained_fp[FR_BULK_SLIME],20000u);
    ASSERT_EQ(wetted.slime_fp,12000u);ASSERT_EQ(wetted.excess_heat,20.);
    ASSERT_EQ(heat,0.);PASS();
}

static void test_invalid_actor_identity_is_transactional(void) {
    fr_bulk_cell_t bulk={.volume_fp={0u,1000u,0u},.capacity_fp=65536u};
    fr_bulk_cell_t after={.capacity_fp=17u};fr_coating_film_t film=rigid_film(),wetted={0};
    film.host.kind=FR_COATING_ACTOR;film.host.generation=0u;
    double heat=123.;uint32_t moved=456u;wetted.oil_fp=789u;
    ASSERT_TRUE(!fr_coating_contact_wet(&bulk,2.,&film,FR_BULK_OIL,500u,
        &after,&heat,&wetted,&moved));
    ASSERT_EQ(after.capacity_fp,17u);ASSERT_EQ(heat,123.);
    ASSERT_EQ(moved,456u);ASSERT_EQ(wetted.oil_fp,789u);PASS();
}

static void test_leaf_and_actor_keep_rest_addresses_across_wetting(void) {
    fr_bulk_cell_t bulk={.volume_fp={0u,2000u,0u},.capacity_fp=65536u};
    fr_coating_film_t film=rigid_film(),after={0};fr_bulk_cell_t remainder={0};
    double heat=0.;uint32_t moved=0u;
    const uint32_t owners[]={FR_COATING_LEAF,FR_COATING_ACTOR};
    for(uint32_t i=0u;i<2u;i++) {
        film.host.kind=owners[i];film.host.id_low=100u+i;
        film.host.generation=25u+i;film.host.rest_x=7u+i;
        ASSERT_TRUE(fr_coating_contact_wet(&bulk,0.,&film,FR_BULK_OIL,500u,
            &remainder,&heat,&after,&moved));
        ASSERT_EQ(moved,500u);
        ASSERT_EQ(memcmp(&after.host,&film.host,sizeof(film.host)),0);
    }
    PASS();
}

static void test_water_cannot_become_combustible_coating(void) {
    fr_bulk_cell_t bulk={.volume_fp={1000u,0u,0u},.capacity_fp=65536u};
    fr_coating_film_t film=rigid_film(),after={.oil_fp=123u};
    fr_bulk_cell_t remainder={.capacity_fp=456u};double heat=789.;uint32_t moved=10u;
    ASSERT_TRUE(!fr_coating_contact_wet(&bulk,0.,&film,FR_BULK_WATER,500u,
        &remainder,&heat,&after,&moved));
    ASSERT_EQ(remainder.capacity_fp,456u);ASSERT_EQ(after.oil_fp,123u);
    ASSERT_EQ(heat,789.);ASSERT_EQ(moved,10u);PASS();
}

static void test_hot_film_drips_under_traction_while_cold_film_holds(void) {
    fr_coating_film_t film=rigid_film(),cold={0},hot={0};
    film.oil_fp=16000u;film.excess_heat=16.;
    fr_bulk_cell_t receiver={.capacity_fp=65536u},cold_bulk={0},hot_bulk={0};
    double cold_heat=0.,hot_heat=0.;uint32_t cold_moved=99u,hot_moved=0u;
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        300.f,5.f,1.f,&cold,&cold_bulk,&cold_heat,&cold_moved));
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        460.f,5.f,1.f,&hot,&hot_bulk,&hot_heat,&hot_moved));
    ASSERT_EQ(cold_moved,0u);ASSERT_TRUE(hot_moved>0u);
    ASSERT_EQ((uint64_t)hot.oil_fp+hot_bulk.volume_fp[FR_BULK_OIL],film.oil_fp);
    ASSERT_TRUE(hot.excess_heat+hot_heat==film.excess_heat);PASS();
}

static void test_full_receiver_does_not_erase_dripping_film(void) {
    fr_coating_film_t film=rigid_film(),after={0};film.slime_fp=10000u;
    fr_bulk_cell_t receiver={.volume_fp={65536u,0u,0u},
        .capacity_fp=65536u},bulk_after={0};
    double heat=0.;uint32_t moved=99u;
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_SLIME,
        470.f,100.f,1.f,&after,&bulk_after,&heat,&moved));
    ASSERT_EQ(moved,0u);ASSERT_EQ(after.slime_fp,film.slime_fp);
    ASSERT_EQ(bulk_after.volume_fp[FR_BULK_WATER],65536u);PASS();
}

static void test_softening_increases_drip_continuously(void) {
    fr_coating_film_t film=rigid_film(),after={0};film.oil_fp=30000u;
    fr_bulk_cell_t receiver={.capacity_fp=65536u},bulk_after={0};
    double heat=0.;uint32_t cold=0u,warm=0u,hot=0u;
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        300.f,6.f,.1f,&after,&bulk_after,&heat,&cold));
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        410.f,6.f,.1f,&after,&bulk_after,&heat,&warm));
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        460.f,6.f,.1f,&after,&bulk_after,&heat,&hot));
    ASSERT_EQ(cold,0u);ASSERT_TRUE(warm>cold&&hot>warm);PASS();
}

static void test_mixed_film_releases_phase_heat_by_thermal_capacity(void) {
    fr_coating_film_t film=rigid_film(),after={0};
    film.oil_fp=10000u;film.slime_fp=10000u;film.excess_heat=47.f;
    fr_bulk_cell_t receiver={.capacity_fp=65536u},bulk_after={0};
    double heat=0.;uint32_t moved=0u;
    ASSERT_TRUE(fr_coating_release(&film,&receiver,0.,FR_BULK_OIL,
        460.f,100.f,1.f,&after,&bulk_after,&heat,&moved));
    ASSERT_EQ(moved,10000u);ASSERT_EQ(after.slime_fp,10000u);
    ASSERT_TRUE(heat>0.&&heat<film.excess_heat);
    ASSERT_EQ(after.excess_heat+heat,film.excess_heat);PASS();
}

static void test_fracture_splits_each_phase_and_heat_without_duplication(void) {
    fr_coating_film_t source=rigid_film(),left={0},right={0};
    source.oil_fp=10001u;source.slime_fp=7001u;source.excess_heat=19.;
    fr_coating_host_t left_host={FR_COATING_RIGID,51u,0u,9u,3u,4u};
    fr_coating_host_t right_host={FR_COATING_RIGID,52u,0u,10u,7u,8u};
    ASSERT_TRUE(fr_coating_film_split(&source,32768u,&left_host,&right_host,
        &left,&right));
    ASSERT_EQ(left.oil_fp+right.oil_fp,source.oil_fp);
    ASSERT_EQ(left.slime_fp+right.slime_fp,source.slime_fp);
    ASSERT_EQ(left.excess_heat+right.excess_heat,source.excess_heat);
    ASSERT_EQ(memcmp(&left.host,&left_host,sizeof(left_host)),0);
    ASSERT_EQ(memcmp(&right.host,&right_host,sizeof(right_host)),0);
    ASSERT_TRUE(left.host.id_low!=right.host.id_low);PASS();
}

static void test_fracture_rejects_duplicate_child_owner_transactionally(void) {
    fr_coating_film_t source=rigid_film(),left={.oil_fp=91u},right={.oil_fp=92u};
    source.oil_fp=100u;fr_coating_host_t child={FR_COATING_RIGID,51u,0u,9u,3u,4u};
    ASSERT_TRUE(!fr_coating_film_split(&source,32768u,&child,&child,&left,&right));
    ASSERT_EQ(left.oil_fp,91u);ASSERT_EQ(right.oil_fp,92u);PASS();
}

static void test_one_quantum_fracture_stays_with_one_child(void) {
    fr_coating_film_t source=rigid_film(),left={0},right={0};
    source.oil_fp=1u;source.excess_heat=2.;
    fr_coating_host_t left_host={FR_COATING_RIGID,51u,0u,9u,3u,4u};
    fr_coating_host_t right_host={FR_COATING_RIGID,52u,0u,10u,7u,8u};
    ASSERT_TRUE(fr_coating_film_split(&source,32768u,&left_host,&right_host,
        &left,&right));
    ASSERT_EQ(left.oil_fp,0u);ASSERT_EQ(right.oil_fp,1u);
    ASSERT_EQ(left.excess_heat,0.);ASSERT_EQ(right.excess_heat,2.);PASS();
}

static void test_many_tiny_contacts_keep_exact_oil_and_jelly_mass(void) {
    uint32_t remainder=0u,oil=0u,jelly=0u,oxidizer=0u;
    fr_coating_species_delta_t delta={0};
    for(uint32_t i=0u;i<10u;i++) {
        ASSERT_TRUE(fr_coating_quantize_volume(FR_BULK_OIL,1u,remainder,&delta));
        remainder=delta.remainder_word;oil+=delta.oil_fuel_fp;
    }
    ASSERT_EQ(oil,11u);ASSERT_EQ(remainder,0u);
    for(uint32_t i=0u;i<50u;i++) {
        ASSERT_TRUE(fr_coating_quantize_volume(FR_BULK_SLIME,1u,remainder,&delta));
        remainder=delta.remainder_word;jelly+=delta.jelly_fuel_fp;
        oxidizer+=delta.jelly_oxidizer_fp;
    }
    ASSERT_EQ(jelly,26u);ASSERT_EQ(oxidizer,39u);
    ASSERT_EQ(remainder,0u);PASS();
}

static void test_corrupt_quantization_word_is_transactional(void) {
    fr_coating_species_delta_t delta={.oil_fuel_fp=77u};
    ASSERT_TRUE(!fr_coating_quantize_volume(FR_BULK_OIL,1u,0x80000000u,&delta));
    ASSERT_EQ(delta.oil_fuel_fp,77u);PASS();
}

int main(void) {
    RUN(test_wetting_debits_mobile_oil_and_preserves_heat);
    RUN(test_retained_slime_does_not_wet_and_existing_film_accumulates);
    RUN(test_invalid_actor_identity_is_transactional);
    RUN(test_leaf_and_actor_keep_rest_addresses_across_wetting);
    RUN(test_water_cannot_become_combustible_coating);
    RUN(test_hot_film_drips_under_traction_while_cold_film_holds);
    RUN(test_full_receiver_does_not_erase_dripping_film);
    RUN(test_softening_increases_drip_continuously);
    RUN(test_mixed_film_releases_phase_heat_by_thermal_capacity);
    RUN(test_fracture_splits_each_phase_and_heat_without_duplication);
    RUN(test_fracture_rejects_duplicate_child_owner_transactionally);
    RUN(test_one_quantum_fracture_stays_with_one_child);
    RUN(test_many_tiny_contacts_keep_exact_oil_and_jelly_mass);
    RUN(test_corrupt_quantization_word_is_transactional);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail>0?1:0;
}
