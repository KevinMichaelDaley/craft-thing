#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/thermal_checkpoint.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static dc_chunk_t *physical(void) {
    dc_chunk_t *chunks=calloc(2,sizeof(*chunks));if(!chunks) return NULL;
    chunks[0].coord=(dc_chunk_coord_t){-5,-7};chunks[1].coord=(dc_chunk_coord_t){9,-11};
    chunks[0].cells[0].fluid_mass=65536;chunks[1].cells[4095].fluid_mass=32768;return chunks;
}
static fr_thermal_checkpoint_t snapshot(void) {
    fr_thermal_checkpoint_t state={.source={9000.25f,200.f},.purpose=FR_THERMAL_WORLD_HEAT_BENCH,.chunk_count=2,.water_excess=calloc(8192,sizeof(float))};
    if(state.water_excess) {state.water_excess[0]=100.125f;state.water_excess[8191]=-20.5f;}return state;
}
static void rehash(uint8_t *bytes,size_t length) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;i++) hash=(hash^((i>=40&&i<48)?0:bytes[i]))*UINT64_C(1099511628211);
    for(uint32_t i=0;i<8;i++) bytes[40+i]=(uint8_t)(hash>>(i*8));
}
static void test_signed_heat_negative_coordinates_and_finite_source_roundtrip_exactly(void) {
    char err[256]={0};dc_chunk_t *chunks=physical();fr_thermal_checkpoint_t source=snapshot(),loaded={0};ASSERT_TRUE(chunks&&source.water_excess);
    uint8_t *bytes=NULL;size_t length=0;
    ASSERT_TRUE(fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    ASSERT_TRUE(length==48u+2u*(16u+4096u*4u)&&memcmp(bytes,"THR1",4)==0);ASSERT_EQ(bytes[16],2u);
    ASSERT_TRUE(fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err)));
    ASSERT_EQ(loaded.purpose,source.purpose);ASSERT_EQ(loaded.chunk_count,source.chunk_count);
    ASSERT_TRUE(memcmp(&loaded.source,&source.source,sizeof(source.source))==0&&memcmp(loaded.water_excess,source.water_excess,8192*sizeof(float))==0);
    fr_thermal_checkpoint_release(&source);fr_thermal_checkpoint_release(&loaded);free(bytes);free(chunks);PASS();
}
static void test_empty_thermal_world_retains_zero_source_without_a_liquid_quota(void) {
    char err[256]={0};fr_thermal_checkpoint_t source={.source={0,200}},loaded={0};uint8_t *bytes=NULL;size_t length=0;
    ASSERT_TRUE(fr_thermal_checkpoint_encode(&source,NULL,0,&bytes,&length,err,sizeof(err))&&length==48u);
    ASSERT_TRUE(fr_thermal_checkpoint_decode(bytes,length,NULL,0,&loaded,err,sizeof(err)));
    ASSERT_EQ(loaded.source.excess_energy,0.f);ASSERT_EQ(loaded.chunk_count,0u);ASSERT_TRUE(!loaded.water_excess);
    fr_thermal_checkpoint_release(&loaded);free(bytes);PASS();
}
static void test_bad_inventory_encode_preserves_the_previous_destination(void) {
    char err[256]={0};dc_chunk_t *chunks=physical();fr_thermal_checkpoint_t source=snapshot();ASSERT_TRUE(chunks&&source.water_excess);
    uint8_t *bytes=(uint8_t *)(uintptr_t)1;size_t length=17;
    source.water_excess[1]=1.f;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));ASSERT_EQ((uintptr_t)bytes,1u);ASSERT_EQ(length,17u);
    source.water_excess[1]=0;source.water_excess[0]=-2000.f;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    source.water_excess[0]=NAN;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    source.water_excess[0]=0;source.source.excess_energy=-1;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    source.source.excess_energy=0;source.source.capacity=0;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    source.source.capacity=200;source.purpose=2;ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    ASSERT_EQ((uintptr_t)bytes,1u);ASSERT_EQ(length,17u);
    fr_thermal_checkpoint_release(&source);free(chunks);PASS();
}
static void test_corruption_future_version_and_wrong_bindings_leave_output_unchanged(void) {
    char err[256]={0};dc_chunk_t *chunks=physical();fr_thermal_checkpoint_t source=snapshot(),loaded,before;ASSERT_TRUE(chunks&&source.water_excess);
    uint8_t *bytes=NULL;size_t length=0;ASSERT_TRUE(fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    memset(&before,0xa5,sizeof(before));loaded=before;
    bytes[length-1]^=1;ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err))&&memcmp(&loaded,&before,sizeof(loaded))==0);bytes[length-1]^=1;
    bytes[4]=2;rehash(bytes,length);ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err))&&memcmp(&loaded,&before,sizeof(loaded))==0);
    bytes[4]=1;rehash(bytes,length);chunks[0].coord.x--;
    ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err))&&memcmp(&loaded,&before,sizeof(loaded))==0);chunks[0].coord.x++;
    chunks[0].cells[0].fluid_mass=0;
    ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err))&&memcmp(&loaded,&before,sizeof(loaded))==0);
    fr_thermal_checkpoint_release(&source);free(bytes);free(chunks);PASS();
}
static void test_truncation_and_appended_data_cannot_hide_liquid_inventory(void) {
    char err[256]={0};dc_chunk_t *chunks=physical();fr_thermal_checkpoint_t source=snapshot(),loaded={0};ASSERT_TRUE(chunks&&source.water_excess);
    uint8_t *bytes=NULL;size_t length=0;ASSERT_TRUE(fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length-1,chunks,2,&loaded,err,sizeof(err)));
    uint8_t *longer=realloc(bytes,length+1);ASSERT_TRUE(longer);bytes=longer;bytes[length]=0;
    ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length+1,chunks,2,&loaded,err,sizeof(err)));
    fr_thermal_checkpoint_release(&source);free(bytes);free(chunks);PASS();
}
static void test_changed_energy_reference_and_duplicate_physical_coordinates_are_rejected(void) {
    char err[256]={0};dc_chunk_t *chunks=physical();fr_thermal_checkpoint_t source=snapshot(),loaded={0};ASSERT_TRUE(chunks&&source.water_excess);
    uint8_t *bytes=NULL;size_t length=0;ASSERT_TRUE(fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    bytes[20]^=1;rehash(bytes,length);ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err)));
    bytes[20]^=1;bytes[24]^=1;rehash(bytes,length);ASSERT_TRUE(!fr_thermal_checkpoint_decode(bytes,length,chunks,2,&loaded,err,sizeof(err)));
    free(bytes);bytes=NULL;chunks[1].coord=chunks[0].coord;
    ASSERT_TRUE(!fr_thermal_checkpoint_encode(&source,chunks,2,&bytes,&length,err,sizeof(err)));
    fr_thermal_checkpoint_release(&source);free(chunks);PASS();
}
int main(void) {RUN(test_signed_heat_negative_coordinates_and_finite_source_roundtrip_exactly);RUN(test_empty_thermal_world_retains_zero_source_without_a_liquid_quota);RUN(test_bad_inventory_encode_preserves_the_previous_destination);RUN(test_corruption_future_version_and_wrong_bindings_leave_output_unchanged);RUN(test_truncation_and_appended_data_cannot_hide_liquid_inventory);RUN(test_changed_energy_reference_and_duplicate_physical_coordinates_are_rejected);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
