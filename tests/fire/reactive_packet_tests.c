#include <stdio.h>
#include <string.h>
#include "ferrum/fire/reactive_state.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static bool input(fr_reactive_record_t records[2]) {
    char err[128];memset(records,0,sizeof(*records)*2u);
    records[0].owner=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={10,2,2,3,0}};
    records[1].owner=fr_reactive_cell_owner((dc_chunk_coord_t){-4,7},9);
    if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,40,&records[0].state,err,sizeof(err))||
       !fr_reactive_state_default(DC_MATERIAL_STONE,176947,8192,600,41,&records[1].state,err,sizeof(err))) return false;
    records[0].state.ignition_threshold=.7f;records[0].state.ignition_integral=.4f;
    records[1].state.coating_oil_fuel_fp=500;records[1].state.coating_threshold=.8f;records[1].state.coating_integral=.5f;
    return true;
}
static void test_memory_packet_matches_existing_file_codec_and_preserves_input(void) {
    char err[128];fr_reactive_record_t records[2],saved[2],out[2];ASSERT_TRUE(input(records));memcpy(saved,records,sizeof(saved));
    uint8_t bytes[312],file_bytes[312];size_t size=0,written=0;
    ASSERT_TRUE(fr_reactive_snapshot_size(2,&size));ASSERT_EQ(size,sizeof(bytes));
    ASSERT_TRUE(fr_reactive_snapshot_encode(records,2,42,bytes,sizeof(bytes),&written,err,sizeof(err)));
    ASSERT_EQ(written,sizeof(bytes));ASSERT_TRUE(memcmp(records,saved,sizeof(saved))==0);
    ASSERT_TRUE(fr_reactive_snapshot_write("build/reactive_packet_compat.frs",records,2,42,err,sizeof(err)));
    FILE *file=fopen("build/reactive_packet_compat.frs","rb");ASSERT_TRUE(file);
    size_t read=fread(file_bytes,1,sizeof(file_bytes),file);int extra=fgetc(file);ASSERT_EQ(fclose(file),0);
    ASSERT_EQ(read,sizeof(file_bytes));ASSERT_EQ(extra,EOF);ASSERT_TRUE(memcmp(bytes,file_bytes,sizeof(bytes))==0);
    uint32_t count=0,required=0;uint64_t next=0;
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,sizeof(bytes),out,2,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(count,2u);ASSERT_EQ(next,42u);ASSERT_EQ(required,2u);
    ASSERT_TRUE(memcmp(out,saved+1,sizeof(*out))==0&&memcmp(out+1,saved,sizeof(*out))==0);PASS();
}
static void test_encode_failure_does_not_touch_the_destination_or_written_size(void) {
    char err[128];fr_reactive_record_t records[2];ASSERT_TRUE(input(records));
    uint8_t bytes[312],saved[312];memset(bytes,0xa5,sizeof(bytes));memcpy(saved,bytes,sizeof(bytes));size_t written=99;
    ASSERT_TRUE(!fr_reactive_snapshot_encode(records,2,42,bytes,sizeof(bytes)-1,&written,err,sizeof(err)));
    ASSERT_EQ(written,99u);ASSERT_TRUE(memcmp(bytes,saved,sizeof(bytes))==0);
    records[1].owner=records[0].owner;
    ASSERT_TRUE(!fr_reactive_snapshot_encode(records,2,42,bytes,sizeof(bytes),&written,err,sizeof(err)));
    ASSERT_EQ(written,99u);ASSERT_TRUE(memcmp(bytes,saved,sizeof(bytes))==0);PASS();
}
static void checksum(uint8_t *bytes,size_t count) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<32;i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    for(size_t i=40;i<count;i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    for(uint32_t i=0;i<8;i++) bytes[32+i]=(uint8_t)(hash>>(i*8u));
}
static void test_capacity_corruption_future_version_and_typed_errors_roll_back(void) {
    char err[128];fr_reactive_record_t records[2],out[2],untouched[2];ASSERT_TRUE(input(records));
    uint8_t bytes[312],valid[312];size_t written=0;
    ASSERT_TRUE(fr_reactive_snapshot_encode(records,2,42,bytes,sizeof(bytes),&written,err,sizeof(err)));memcpy(valid,bytes,sizeof(bytes));
    memset(out,0xab,sizeof(out));memcpy(untouched,out,sizeof(out));uint32_t count=77,required=88;uint64_t next=99;
    ASSERT_TRUE(!fr_reactive_snapshot_decode(bytes,sizeof(bytes),out,1,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(required,2u);ASSERT_EQ(count,77u);ASSERT_EQ(next,99u);ASSERT_TRUE(memcmp(out,untouched,sizeof(out))==0);
    for(uint32_t kind=0;kind<4;kind++) {
        memcpy(bytes,valid,sizeof(bytes));required=88;
        if(kind==0) bytes[100]^=1;
        if(kind==1) bytes[4]=(uint8_t)(FR_REACTIVE_STATE_ABI+1u);
        if(kind==2) {bytes[40]=99;checksum(bytes,sizeof(bytes));}
        size_t length=kind==3?sizeof(bytes)-1:sizeof(bytes);
        ASSERT_TRUE(!fr_reactive_snapshot_decode(bytes,length,out,2,&count,&next,&required,err,sizeof(err)));
        ASSERT_EQ(count,77u);ASSERT_EQ(next,99u);ASSERT_EQ(required,88u);ASSERT_TRUE(memcmp(out,untouched,sizeof(out))==0);
    }
    PASS();
}
static void test_empty_packet_retains_the_ambient_old_world_contract(void) {
    char err[128];uint8_t bytes[40];size_t size=0,written=0;uint32_t count=9,required=9;uint64_t next=9;
    ASSERT_TRUE(fr_reactive_snapshot_size(0,&size));ASSERT_EQ(size,sizeof(bytes));
    ASSERT_TRUE(fr_reactive_snapshot_encode(NULL,0,1,bytes,sizeof(bytes),&written,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes,sizeof(bytes),NULL,0,&count,&next,&required,err,sizeof(err)));
    ASSERT_EQ(count,0u);ASSERT_EQ(required,0u);ASSERT_EQ(next,1u);PASS();
}
int main(void) {
    RUN(test_memory_packet_matches_existing_file_codec_and_preserves_input);
    RUN(test_encode_failure_does_not_touch_the_destination_or_written_size);
    RUN(test_capacity_corruption_future_version_and_typed_errors_roll_back);
    RUN(test_empty_packet_retains_the_ambient_old_world_contract);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
