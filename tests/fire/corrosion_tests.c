#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/corrosion.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s [%s]\n",__FILE__,__LINE__,#expr,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_gold_greek_corrosion_uses_finite_bound_oxidizer_and_keeps_origin(void) {
    char err[256]={0};fr_reactive_state_t source,after;
    fr_corrosion_products_t products;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,65536u,0u,800.f,4001u,
        &source,err,sizeof(err)));
    source.coating_jelly_fuel_fp=65536u;source.coating_oxidizer_fp=8000u;
    source.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&source,800.f);
    uint64_t initial_mass=fr_reactive_state_mass_fp(&source);
    ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,.5f,
        &after,&products,err,sizeof(err)));
    ASSERT_TRUE(products.converted_fp>0u&&products.converted_fp<65536u);
    ASSERT_EQ(products.residue_fp,products.converted_fp+products.oxidizer_fp);
    ASSERT_EQ(source.coating_oxidizer_fp-after.coating_oxidizer_fp,products.oxidizer_fp);
    ASSERT_EQ(after.core_oxidizer_fp-source.core_oxidizer_fp,products.oxidizer_fp);
    ASSERT_EQ(fr_reactive_state_mass_fp(&after),initial_mass);
    ASSERT_EQ(after.origin_id_low,source.origin_id_low);
    ASSERT_EQ(after.material,DC_MATERIAL_GOLD);
    ASSERT_TRUE(fr_corrosion_pristine_mass_fp(&after)<source.dry_mass_fp);
    ASSERT_TRUE(products.section_strength_fraction<1.f);
    PASS();
}

static void test_ordinary_oxygen_wetness_and_gold_immunity(void) {
    char err[256]={0};fr_reactive_state_t stone,gold,after;
    fr_corrosion_products_t products;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,65536u,0u,500.f,4002u,
        &stone,err,sizeof(err)));
    ASSERT_TRUE(fr_corrosion_reference(&stone,FR_CORROSION_AIR,1000u,.21f,0.f,100.f,
        &after,&products,err,sizeof(err)));
    ASSERT_EQ(after.oxidized_fraction,0.f);
    ASSERT_TRUE(fr_corrosion_reference(&stone,FR_CORROSION_AIR,0u,.21f,1.f,100.f,
        &after,&products,err,sizeof(err)));
    ASSERT_EQ(after.oxidized_fraction,0.f);
    ASSERT_TRUE(fr_corrosion_reference(&stone,FR_CORROSION_AIR,1000u,.21f,1.f,100.f,
        &after,&products,err,sizeof(err)));
    ASSERT_TRUE(after.oxidized_fraction>0.f&&products.oxidizer_fp>0u);
    ASSERT_EQ(fr_reactive_state_mass_fp(&after),
        fr_reactive_state_mass_fp(&stone)+products.oxidizer_fp);
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,65536u,0u,500.f,4003u,
        &gold,err,sizeof(err)));
    ASSERT_TRUE(fr_corrosion_reference(&gold,FR_CORROSION_AIR,1000u,.21f,1.f,100.f,
        &after,&products,err,sizeof(err)));
    ASSERT_EQ(after.corroded_fraction,0.f);
    ASSERT_EQ(after.material,DC_MATERIAL_GOLD);
    PASS();
}

static void test_full_conversion_makes_irreversible_residue_and_spent_ingredient(void) {
    char err[256]={0};fr_reactive_state_t source,after,retry;
    fr_corrosion_products_t products;
    for(uint32_t kind=0u;kind<2u;kind++) {
        uint32_t material=kind?DC_MATERIAL_MAGIC_INGREDIENT:DC_MATERIAL_GOLD;
        ASSERT_TRUE(fr_reactive_state_default(material,65536u,0u,800.f,4004u+kind,
            &source,err,sizeof(err)));
        source.coating_jelly_fuel_fp=65536u;source.coating_oxidizer_fp=10000u;
        source.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&source,800.f);
        uint64_t initial_mass=fr_reactive_state_mass_fp(&source);
        ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,2.f,
            &after,&products,err,sizeof(err)));
        ASSERT_EQ(after.material,kind?DC_MATERIAL_ASH:DC_MATERIAL_CORROSION_RESIDUE);
        ASSERT_TRUE((after.flags&FR_REACTIVE_SPENT)!=0u);
        ASSERT_EQ(fr_corrosion_pristine_mass_fp(&after),0u);
        ASSERT_EQ(after.fuel_mass_fp,0u);
        ASSERT_EQ(fr_reactive_state_mass_fp(&after),initial_mass);
        ASSERT_EQ(after.origin_id_low,source.origin_id_low);
        ASSERT_TRUE(fr_corrosion_reference(&after,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,2.f,
            &retry,&products,err,sizeof(err)));
        ASSERT_EQ(memcmp(&retry,&after,sizeof(after)),0);
    }
    PASS();
}
static void test_equal_time_corrosion_and_oxidizer_cap_preserve_stock(void) {
    char err[256]={0};fr_reactive_state_t source,whole,half,next,capped;
    fr_corrosion_products_t products;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,65536u,0u,800.f,4006u,
        &source,err,sizeof(err)));
    source.coating_jelly_fuel_fp=65536u;source.coating_oxidizer_fp=8000u;
    source.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&source,800.f);
    ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,.5f,
        &whole,&products,err,sizeof(err)));
    half=source;
    for(uint32_t i=0u;i<2u;i++) {
        ASSERT_TRUE(fr_corrosion_reference(&half,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,.25f,
            &next,&products,err,sizeof(err)));
        half=next;
    }
    ASSERT_EQ(whole.corroded_fraction,half.corroded_fraction);
    ASSERT_EQ(whole.coating_oxidizer_fp,half.coating_oxidizer_fp);
    ASSERT_EQ(fr_reactive_state_mass_fp(&whole),fr_reactive_state_mass_fp(&half));
    source.coating_oxidizer_fp=100u;
    ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,2.f,
        &capped,&products,err,sizeof(err)));
    ASSERT_EQ(products.oxidizer_fp,100u);
    ASSERT_EQ(capped.coating_oxidizer_fp,0u);
    ASSERT_TRUE(capped.corroded_fraction<.02f);
    ASSERT_EQ(fr_reactive_state_mass_fp(&capped),fr_reactive_state_mass_fp(&source));
    PASS();
}

static void test_residue_import_and_recraft_cannot_relabel_spent_lineage(void) {
    char err[256]={0};fr_reactive_state_t source,residue,forged;
    fr_corrosion_products_t products;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,65536u,0u,800.f,4007u,
        &source,err,sizeof(err)));
    source.coating_jelly_fuel_fp=65536u;source.coating_oxidizer_fp=10000u;
    source.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&source,800.f);
    ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,2.f,
        &residue,&products,err,sizeof(err)));
    forged=residue;forged.material=DC_MATERIAL_GOLD;
    ASSERT_TRUE(!fr_reactive_state_valid(&forged,err,sizeof(err)));
    forged=residue;forged.corroded_fraction=0.f;
    ASSERT_TRUE(!fr_reactive_state_valid(&forged,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_valid(&residue,err,sizeof(err)));
    PASS();
}
static void test_partial_corrosion_survives_fracture_and_snapshot(void) {
    char err[256]={0};fr_reactive_state_t source,partial,left,right;
    fr_corrosion_products_t products;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,65536u,0u,800.f,4010u,
        &source,err,sizeof(err)));
    source.coating_jelly_fuel_fp=65536u;source.coating_oxidizer_fp=8000u;
    source.thermal_energy=(float)fr_reactive_state_energy_at_temperature(&source,800.f);
    ASSERT_TRUE(fr_corrosion_reference(&source,FR_CORROSION_GREEK_FIRE,0u,0.f,1.f,.5f,
        &partial,&products,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_split(&partial,DC_FLUID_FULL/2u,4011u,4012u,
        &left,&right,err,sizeof(err)));
    ASSERT_EQ(left.origin_id_low,source.origin_id_low);
    ASSERT_EQ(right.origin_id_low,source.origin_id_low);
    ASSERT_EQ(left.corroded_fraction,partial.corroded_fraction);
    ASSERT_EQ(right.corroded_fraction,partial.corroded_fraction);
    ASSERT_EQ(fr_reactive_state_mass_fp(&left)+fr_reactive_state_mass_fp(&right),
        fr_reactive_state_mass_fp(&partial));
    fr_reactive_record_t original={.owner={.kind=FR_REACTIVE_OWNER_BODY,.key={10u,1u,20u,30u,0u}},.state=left};
    size_t size=0,written=0;ASSERT_TRUE(fr_reactive_snapshot_size(1u,&size));
    uint8_t *bytes=malloc(size);ASSERT_TRUE(bytes);
    ASSERT_TRUE(fr_reactive_snapshot_encode(&original,1u,4013u,bytes,size,&written,err,sizeof(err)));
    fr_reactive_record_t restored={0};uint32_t count=0;uint64_t next=0;
    uint32_t required=0;
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,written,&restored,1u,&count,&next,&required,
        err,sizeof(err)));
    ASSERT_EQ(count,1u);ASSERT_EQ(restored.state.origin_id_low,source.origin_id_low);
    ASSERT_EQ(restored.state.corroded_fraction,partial.corroded_fraction);
    free(bytes);PASS();
}

int main(void) {
    RUN(test_gold_greek_corrosion_uses_finite_bound_oxidizer_and_keeps_origin);
    RUN(test_ordinary_oxygen_wetness_and_gold_immunity);
    RUN(test_full_conversion_makes_irreversible_residue_and_spent_ingredient);
    RUN(test_equal_time_corrosion_and_oxidizer_cap_preserve_stock);
    RUN(test_residue_import_and_recraft_cannot_relabel_spent_lineage);
    RUN(test_partial_corrosion_survives_fracture_and_snapshot);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
