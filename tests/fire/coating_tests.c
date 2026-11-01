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

int main(void) {
    RUN(test_wetting_debits_mobile_oil_and_preserves_heat);
    RUN(test_retained_slime_does_not_wet_and_existing_film_accumulates);
    RUN(test_invalid_actor_identity_is_transactional);
    RUN(test_leaf_and_actor_keep_rest_addresses_across_wetting);
    RUN(test_water_cannot_become_combustible_coating);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail>0?1:0;
}
