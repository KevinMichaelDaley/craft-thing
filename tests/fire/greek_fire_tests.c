#include <stdio.h>

#include "ferrum/fire/greek_fire.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s [%s]\n",__FILE__,__LINE__,#expr,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static fr_reactive_state_t coated(uint32_t id,bool mixed,bool wet,char *err,uint32_t cap) {
    fr_reactive_state_t state={0};
    if(!fr_reactive_state_default(DC_MATERIAL_STONE,65536u,wet?65536u:0u,800.f,id,
        &state,err,cap)) return state;
    state.coating_oil_fuel_fp=mixed?32768u:65536u;
    state.coating_jelly_fuel_fp=mixed?32768u:0u;
    state.coating_oxidizer_fp=mixed?32768u:0u;
    state.coating_threshold=.01f;state.coating_integral=.02f;
    state.flags|=FR_REACTIVE_COATING_FLAMING;
    state.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&state,800.f);
    return state;
}

static void test_mixed_film_burns_underwater_with_finite_bound_stock(void) {
    char err[256]={0};fr_reactive_state_t source=coated(501u,true,true,err,sizeof(err)),after;
    fr_coating_combustion_products_t products;
    uint64_t initial=fr_reactive_state_mass_fp(&source);
    ASSERT_TRUE(fr_coating_combustion_reference(&source,0u,1.f,5.f,&after,&products,err,sizeof(err)));
    ASSERT_TRUE(products.oil_fp>0u&&products.jelly_fp>0u&&products.bound_oxidizer_fp>0u);
    ASSERT_EQ(products.ambient_oxygen_fp,0u);
    ASSERT_EQ(after.coating_oil_fuel_fp+products.oil_fp,source.coating_oil_fuel_fp);
    ASSERT_EQ(after.coating_jelly_fuel_fp+products.jelly_fp,source.coating_jelly_fuel_fp);
    ASSERT_EQ(after.coating_oxidizer_fp+products.bound_oxidizer_fp,source.coating_oxidizer_fp);
    ASSERT_EQ(after.dry_mass_fp,source.dry_mass_fp);
    ASSERT_EQ(initial,fr_reactive_state_mass_fp(&after)+products.soot_fp+products.exhaust_fp);
    ASSERT_TRUE(products.released_heat>0.);
    PASS();
}

static void test_mixed_film_depletes_faster_than_matched_oil_while_wet_oil_douses(void) {
    char err[256]={0};fr_reactive_state_t greek=coated(502u,true,false,err,sizeof(err));
    fr_reactive_state_t oil=coated(503u,false,false,err,sizeof(err));
    fr_reactive_state_t wet_oil=coated(504u,false,true,err,sizeof(err));
    fr_reactive_state_t a,b,c;fr_coating_combustion_products_t gp,op,wp;
    ASSERT_TRUE(fr_coating_combustion_reference(&greek,65536u,1.f,10.f,&a,&gp,err,sizeof(err)));
    ASSERT_TRUE(fr_coating_combustion_reference(&oil,65536u,1.f,10.f,&b,&op,err,sizeof(err)));
    ASSERT_TRUE(fr_coating_combustion_reference(&wet_oil,65536u,1.f,10.f,&c,&wp,err,sizeof(err)));
    ASSERT_TRUE(gp.oil_fp+gp.jelly_fp>2u*op.oil_fp);
    ASSERT_EQ(wp.oil_fp,0u);
    ASSERT_EQ(c.coating_oil_fuel_fp,wet_oil.coating_oil_fuel_fp);
    PASS();
}

static void test_exhausted_bound_oxidizer_extinguishes_mixed_film(void) {
    char err[256]={0};fr_reactive_state_t source=coated(505u,true,true,err,sizeof(err)),after,again;
    fr_coating_combustion_products_t products;
    source.coating_oxidizer_fp=100u;
    ASSERT_TRUE(fr_coating_combustion_reference(&source,0u,1.f,100.f,&after,&products,err,sizeof(err)));
    ASSERT_EQ(after.coating_oxidizer_fp,0u);
    ASSERT_TRUE((after.flags&FR_REACTIVE_COATING_FLAMING)==0u);
    ASSERT_TRUE(fr_coating_combustion_reference(&after,0u,1.f,100.f,&again,&products,err,sizeof(err)));
    ASSERT_EQ(products.oil_fp+products.jelly_fp,0u);
    PASS();
}
static void test_unmixed_jelly_cannot_spend_nonexistent_oil(void) {
    char err[256]={0};fr_reactive_state_t source=coated(506u,true,false,err,sizeof(err)),after;
    fr_coating_combustion_products_t products;
    source.coating_oil_fuel_fp=0u;
    ASSERT_TRUE(fr_coating_combustion_reference(&source,65536u,1.f,10.f,
        &after,&products,err,sizeof(err)));
    ASSERT_EQ(after.coating_oil_fuel_fp,0u);
    ASSERT_EQ(after.coating_jelly_fuel_fp,source.coating_jelly_fuel_fp);
    ASSERT_EQ(products.oil_fp+products.jelly_fp,0u);
    PASS();
}

static void test_equal_time_cadences_consume_the_same_finite_mixture(void) {
    char err[256]={0};fr_reactive_state_t source=coated(507u,true,false,err,sizeof(err));
    fr_reactive_state_t whole,part,next;fr_coating_combustion_products_t products;
    ASSERT_TRUE(fr_coating_combustion_reference(&source,0u,1.f,10.f,
        &whole,&products,err,sizeof(err)));
    part=source;
    for(uint32_t i=0u;i<10u;i++) {
        ASSERT_TRUE(fr_coating_combustion_reference(&part,0u,1.f,1.f,
            &next,&products,err,sizeof(err)));
        part=next;
    }
    int32_t difference=(int32_t)(whole.coating_oil_fuel_fp+whole.coating_jelly_fuel_fp)-
        (int32_t)(part.coating_oil_fuel_fp+part.coating_jelly_fuel_fp);
    ASSERT_TRUE(difference>=-2&&difference<=2);
    PASS();
}

int main(void) {
    RUN(test_mixed_film_burns_underwater_with_finite_bound_stock);
    RUN(test_mixed_film_depletes_faster_than_matched_oil_while_wet_oil_douses);
    RUN(test_exhausted_bound_oxidizer_extinguishes_mixed_film);
    RUN(test_unmixed_jelly_cannot_spend_nonexistent_oil);
    RUN(test_equal_time_cadences_consume_the_same_finite_mixture);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
