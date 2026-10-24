#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/leaf_material.h"
#include "ferrum/fire/reactive_state.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_default_composition_preserves_exact_mass_and_temperature(void) {
    fr_reactive_state_t s;char err[128];float temperature=0;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GREEK_FIRE,78643,0,1000,9,&s,err,sizeof(err)));
    ASSERT_EQ(fr_reactive_state_mass_fp(&s),78643u);
    ASSERT_EQ(s.dry_mass_fp,78643u);ASSERT_TRUE(s.core_oxidizer_fp>0);
    ASSERT_TRUE(fr_reactive_state_temperature(&s,&temperature,err,sizeof(err)));
    ASSERT_TRUE(fabsf(temperature-1000)<.001f);
    ASSERT_EQ(s.parcel_id_low,9u);ASSERT_EQ(s.origin_id_low,9u);PASS();
}
static void test_soil_moisture_and_leaf_pigment_have_different_meanings(void) {
    char err[128];fr_reactive_state_t soil,leaf;
    dc_mpm_particle_t p={.material=DC_MATERIAL_DIRT,.mass_fp=78643,.flags=8192,.id_lo=4,.id_hi=8};
    ASSERT_TRUE(fr_reactive_state_from_particle(&p,293.15f,1,&soil,err,sizeof(err)));
    ASSERT_EQ(soil.bound_water_fp,8192u);ASSERT_TRUE((soil.flags&FR_REACTIVE_WATER_MIRROR)!=0);
    ASSERT_EQ(fr_reactive_state_mass_fp(&soil),86835u);
    p.material=DC_MATERIAL_LEAF_DEBRIS;p.mass_fp=13107;p.flags=DC_LEAF_COLOR_FLAG|0x33ff77u;
    dc_mpm_particle_t before=p;
    ASSERT_TRUE(fr_reactive_state_from_particle(&p,600,2,&leaf,err,sizeof(err)));
    ASSERT_EQ(leaf.bound_water_fp,0u);ASSERT_EQ(leaf.fuel_mass_fp,13107u);
    ASSERT_EQ(memcmp(&before,&p,sizeof(p)),0);PASS();
}
static void test_splits_conserve_species_energy_and_hazard_without_rerolling(void) {
    char err[128];fr_reactive_state_t s,left,right;float t=0;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,1000,700,10,&s,err,sizeof(err)));
    s.coating_oil_fuel_fp=1500;s.coating_jelly_fuel_fp=700;s.coating_oxidizer_fp=300;
    s.ignition_threshold=1.25f;s.ignition_integral=.75f;s.coating_threshold=2;s.coating_integral=.5f;
    s.pending_shock_energy=12;
    ASSERT_TRUE(fr_reactive_state_valid(&s,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_split(&s,21845,11,12,&left,&right,err,sizeof(err)));
    ASSERT_EQ(fr_reactive_state_mass_fp(&left)+fr_reactive_state_mass_fp(&right),fr_reactive_state_mass_fp(&s));
    ASSERT_EQ(left.fuel_mass_fp+right.fuel_mass_fp,s.fuel_mass_fp);
    ASSERT_EQ(left.coating_oil_fuel_fp+right.coating_oil_fuel_fp,s.coating_oil_fuel_fp);
    ASSERT_TRUE(fabsf((left.thermal_energy+right.thermal_energy)-s.thermal_energy)<.001f);
    ASSERT_EQ(left.ignition_threshold,s.ignition_threshold);ASSERT_EQ(right.ignition_integral,s.ignition_integral);
    ASSERT_EQ(left.origin_id_low,10u);ASSERT_EQ(right.origin_id_low,10u);
    ASSERT_EQ(left.parcel_id_low,11u);ASSERT_EQ(right.parcel_id_low,12u);
    ASSERT_TRUE(fr_reactive_state_temperature(&left,&t,err,sizeof(err)));
    ASSERT_TRUE(t>0);PASS();
}
static void test_failed_state_change_leaves_both_outputs_unchanged(void) {
    char err[128];fr_reactive_state_t s,left={.material=77},right={.material=88};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,10,&s,err,sizeof(err)));
    fr_reactive_state_t a=left,b=right;
    ASSERT_TRUE(!fr_reactive_state_split(&s,65537,11,12,&left,&right,err,sizeof(err)));
    ASSERT_EQ(memcmp(&left,&a,sizeof(a)),0);ASSERT_EQ(memcmp(&right,&b,sizeof(b)),0);
    s.thermal_energy=NAN;ASSERT_TRUE(!fr_reactive_state_valid(&s,err,sizeof(err)));PASS();
}
static void test_negative_world_owner_and_generation_keys_are_exact(void) {
    char err[128];fr_reactive_owner_t o=fr_reactive_cell_owner((dc_chunk_coord_t){-123456789,-1},4095);
    ASSERT_TRUE(fr_reactive_owner_valid(&o,err,sizeof(err)));
    ASSERT_EQ(o.kind,FR_REACTIVE_OWNER_CELL);
    ASSERT_EQ(o.key[1],UINT32_MAX);ASSERT_EQ(o.key[2],UINT32_MAX);ASSERT_EQ(o.key[3],UINT32_MAX);
    ASSERT_EQ(o.key[4],4095u);o.key[4]=4096;
    ASSERT_TRUE(!fr_reactive_owner_valid(&o,err,sizeof(err)));
    o=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={19,2,4,8,0}};
    ASSERT_TRUE(fr_reactive_owner_valid(&o,err,sizeof(err)));
    o.key[1]=0;ASSERT_TRUE(!fr_reactive_owner_valid(&o,err,sizeof(err)));PASS();
}
static void test_snapshot_roundtrip_and_capacity_failure_are_transactional(void) {
    const char *path="build/reactive_state_roundtrip.bin";char err[128];
    fr_reactive_record_t input[2]={0},out[2]={0};
    input[0].owner=fr_reactive_cell_owner((dc_chunk_coord_t){-3,7},31);
    input[1].owner=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={19,2,4,8,0}};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,750,1,&input[0].state,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_GOLD,1264845,0,850,2,&input[1].state,err,sizeof(err)));
    input[1].state.corroded_fraction=.25f;input[1].state.flags|=FR_REACTIVE_SPENT;
    ASSERT_TRUE(fr_reactive_snapshot_write(path,input,2,3,err,sizeof(err)));
    uint32_t count=91,required=0;uint64_t next=92;
    memset(out,0x5a,sizeof(out));fr_reactive_record_t saved[2];memcpy(saved,out,sizeof(out));
    ASSERT_TRUE(!fr_reactive_snapshot_read(path,out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(required,2u);ASSERT_EQ(count,91u);ASSERT_EQ(next,92u);
    ASSERT_EQ(memcmp(out,saved,sizeof(out)),0);
    ASSERT_TRUE(fr_reactive_snapshot_read(path,out,2,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(count,2u);ASSERT_EQ(next,3u);ASSERT_EQ(memcmp(out,input,sizeof(out)),0);
    remove(path);PASS();
}
static void test_corrupt_or_future_snapshot_cannot_publish_partial_state(void) {
    const char *path="build/reactive_state_corrupt.bin";char err[128];
    fr_reactive_record_t record={.owner={.kind=FR_REACTIVE_OWNER_BODY,.key={19,2,4,8,0}}},out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,750,1,&record.state,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_snapshot_write(path,&record,1,2,err,sizeof(err)));
    FILE *file=fopen(path,"r+b");ASSERT_TRUE(file);ASSERT_EQ(fseek(file,-1,SEEK_END),0);
    int value=fgetc(file);ASSERT_TRUE(value!=EOF);ASSERT_EQ(fseek(file,-1,SEEK_END),0);
    ASSERT_TRUE(fputc(value^1,file)!=EOF);ASSERT_EQ(fclose(file),0);
    memset(&out,0x5a,sizeof(out));fr_reactive_record_t saved=out;uint32_t count=4,required=0;uint64_t next=5;
    ASSERT_TRUE(!fr_reactive_snapshot_read(path,&out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);ASSERT_EQ(count,4u);ASSERT_EQ(next,5u);
    ASSERT_TRUE(fr_reactive_snapshot_write(path,&record,1,2,err,sizeof(err)));
    file=fopen(path,"r+b");ASSERT_TRUE(file);ASSERT_EQ(fseek(file,4,SEEK_SET),0);
    ASSERT_TRUE(fputc(2,file)!=EOF);ASSERT_EQ(fclose(file),0);
    ASSERT_TRUE(!fr_reactive_snapshot_read(path,&out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);remove(path);PASS();
}
static void test_duplicate_owners_or_parcel_ids_are_not_saved(void) {
    const char *path="build/reactive_state_duplicate.bin";char err[128];
    fr_reactive_record_t a[2]={{.owner={.kind=FR_REACTIVE_OWNER_BODY,.key={19,2,4,8,0}}}};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,750,1,&a[0].state,err,sizeof(err)));
    a[1]=a[0];
    ASSERT_TRUE(!fr_reactive_snapshot_write(path,a,2,3,err,sizeof(err)));
    a[1].owner.key[2]++;
    ASSERT_TRUE(!fr_reactive_snapshot_write(path,a,2,3,err,sizeof(err)));
    a[1].state.parcel_id_low=2;
    ASSERT_TRUE(fr_reactive_snapshot_write(path,a,2,3,err,sizeof(err)));
    ASSERT_TRUE(!fr_reactive_snapshot_write(path,a,2,2,err,sizeof(err)));
    remove(path);PASS();
}
static void test_snapshot_counter_cannot_reuse_a_preserved_origin_identity(void) {
    const char *path="build/reactive_origin_counter.bin";char err[128];
    fr_reactive_record_t record={.owner={.kind=FR_REACTIVE_OWNER_BODY,.key={19,2,4,8,0}}};
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,750,5,&record.state,err,sizeof(err)));
    record.state.origin_id_low=100;
    ASSERT_TRUE(!fr_reactive_snapshot_write(path,&record,1,6,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_snapshot_write(path,&record,1,101,err,sizeof(err)));
    remove(path);PASS();
}
int main(void) {
    _Static_assert(sizeof(fr_reactive_state_t)==FR_REACTIVE_STATE_WORDS*4u,"Reactive state ABI changed");
    _Static_assert(offsetof(fr_reactive_state_t,thermal_energy)==68u,"Reactive energy offset changed");
    RUN(test_default_composition_preserves_exact_mass_and_temperature);
    RUN(test_soil_moisture_and_leaf_pigment_have_different_meanings);
    RUN(test_splits_conserve_species_energy_and_hazard_without_rerolling);
    RUN(test_failed_state_change_leaves_both_outputs_unchanged);
    RUN(test_negative_world_owner_and_generation_keys_are_exact);
    RUN(test_snapshot_roundtrip_and_capacity_failure_are_transactional);
    RUN(test_corrupt_or_future_snapshot_cannot_publish_partial_state);
    RUN(test_duplicate_owners_or_parcel_ids_are_not_saved);
    RUN(test_snapshot_counter_cannot_reuse_a_preserved_origin_identity);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
