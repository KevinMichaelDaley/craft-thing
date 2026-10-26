#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/reactive_state.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_sub_ulp_heat_is_retained_and_temperature_uses_full_inventory(void) {
    char err[256]={0};fr_reactive_state_t s;
    ASSERT_EQ(FR_REACTIVE_STATE_ABI,4u);
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,350,1,&s,err,sizeof(err)));
    double before=fr_reactive_state_energy(&s);
    for(uint32_t i=0;i<100000;i++) ASSERT_TRUE(fr_reactive_state_add_heat(&s,1e-6,err,sizeof(err)));
    printf("Repeated sub-ULP heat error %.12g\n",fr_reactive_state_energy(&s)-before-.1);
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&s)-before-.1)<1e-8);
    ASSERT_TRUE(s.thermal_energy_residual!=0.f&&fr_reactive_state_valid(&s,err,sizeof(err)));
    float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(&s,&temperature,err,sizeof(err)));
    ASSERT_TRUE(fabs(temperature-fr_reactive_state_energy(&s)/fr_reactive_state_heat_capacity(&s))<3e-5);PASS();
}
static void rehash(uint8_t *bytes,size_t length) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;i++) if(i<32||i>=40) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    for(uint32_t i=0;i<8;i++) bytes[32+i]=(uint8_t)(hash>>(8*i));
}
static void test_abi_two_migrates_zero_remainder_but_rejects_nonzero_old_reserved_bits(void) {
    char err[256]={0};fr_reactive_record_t record={.owner={.kind=FR_REACTIVE_OWNER_CELL,.key={0,0,0,0,3}}},out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,1,&record.state,err,sizeof(err)));
    size_t length=0,written=0;ASSERT_TRUE(fr_reactive_snapshot_size(1,&length));uint8_t *bytes=malloc(length);ASSERT_TRUE(bytes);
    ASSERT_TRUE(fr_reactive_snapshot_encode(&record,1,2,bytes,length,&written,err,sizeof(err)));
    bytes[4]=2;rehash(bytes,length);uint32_t count=0,required=0;uint64_t next=0;
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,length,&out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(out.state.thermal_energy_residual,0.f);ASSERT_EQ(memcmp(&out,&record,sizeof(record)),0);
    uint32_t value=0;float residual=1e-6f;memcpy(&value,&residual,4);
    for(uint32_t word=25;word<=26;word++) {
        for(uint32_t i=0;i<4;i++) bytes[40+(FR_REACTIVE_OWNER_WORDS+word)*4+i]=(uint8_t)(value>>(8*i));
        rehash(bytes,length);fr_reactive_record_t saved=out;count=91;next=92;
        ASSERT_TRUE(!fr_reactive_snapshot_decode(bytes,length,&out,1,&count,&next,&required,err,sizeof(err)));
        ASSERT_EQ(memcmp(&out,&saved,sizeof(out)),0);ASSERT_EQ(count,91u);ASSERT_EQ(next,92u);
        memset(bytes+40+(FR_REACTIVE_OWNER_WORDS+word)*4,0,4);
    }
    free(bytes);PASS();
}
static void test_splits_and_snapshots_preserve_the_full_energy_expansion(void) {
    char err[256]={0};fr_reactive_record_t record={.owner={.kind=FR_REACTIVE_OWNER_BODY,.key={7,1,2,3,0}}},out;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,1000,350,1,&record.state,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_add_heat(&record.state,1e-5+1e-13,err,sizeof(err)));
    ASSERT_TRUE(record.state.thermal_energy_residual!=0.f&&record.state.thermal_energy_correction!=0.f);
    fr_reactive_state_t a,b;ASSERT_TRUE(fr_reactive_state_split(&record.state,21845,2,3,&a,&b,err,sizeof(err)));
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&a)+fr_reactive_state_energy(&b)-fr_reactive_state_energy(&record.state))<1e-10);
    size_t length=0,written=0;ASSERT_TRUE(fr_reactive_snapshot_size(1,&length));uint8_t *bytes=malloc(length);ASSERT_TRUE(bytes);
    ASSERT_TRUE(fr_reactive_snapshot_encode(&record,1,2,bytes,length,&written,err,sizeof(err)));
    uint32_t count=0,required=0;uint64_t next=0;
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,length,&out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(memcmp(&record,&out,sizeof(record)),0);free(bytes);PASS();
}
static void test_invalid_heat_or_residual_leaves_state_unchanged(void) {
    char err[256]={0};fr_reactive_state_t s;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,1,&s,err,sizeof(err)));
    fr_reactive_state_t saved=s;
    ASSERT_TRUE(!fr_reactive_state_add_heat(&s,NAN,err,sizeof(err)));ASSERT_EQ(memcmp(&saved,&s,sizeof(s)),0);
    ASSERT_TRUE(!fr_reactive_state_add_heat(&s,-10000,err,sizeof(err)));ASSERT_EQ(memcmp(&saved,&s,sizeof(s)),0);
    ASSERT_TRUE(!fr_reactive_state_add_heat(&s,DBL_MAX,err,sizeof(err)));ASSERT_EQ(memcmp(&saved,&s,sizeof(s)),0);
    s.thermal_energy_residual=NAN;ASSERT_TRUE(!fr_reactive_state_valid(&s,err,sizeof(err)));
    s.thermal_energy_residual=1;ASSERT_TRUE(!fr_reactive_state_valid(&s,err,sizeof(err)));
    s=saved;s.thermal_energy_correction=NAN;ASSERT_TRUE(!fr_reactive_state_valid(&s,err,sizeof(err)));
    s.thermal_energy_correction=1;ASSERT_TRUE(!fr_reactive_state_valid(&s,err,sizeof(err)));PASS();
}
int main(void) {
    RUN(test_sub_ulp_heat_is_retained_and_temperature_uses_full_inventory);
    RUN(test_splits_and_snapshots_preserve_the_full_energy_expansion);
    RUN(test_invalid_heat_or_residual_leaves_state_unchanged);
    RUN(test_abi_two_migrates_zero_remainder_but_rejects_nonzero_old_reserved_bits);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
