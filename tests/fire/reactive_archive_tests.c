#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/reactive_archive.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static bool fixture(dc_chunk_t *physical,fr_reactive_archive_t *archive,fr_reactive_record_t *body,
    fr_reactive_chunk_t channels[2],fr_reactive_attachment_t attachments[2],char *err,uint32_t cap) {
    memset(physical,0,2*sizeof(*physical));memset(attachments,0,2*sizeof(*attachments));memset(body,0,sizeof(*body));
    physical[0].coord=(dc_chunk_coord_t){-5,-7};physical[1].coord=(dc_chunk_coord_t){-4,-7};
    for(uint32_t i=0;i<2;i++) {
        physical[i].cells[0].material=DC_MATERIAL_WOOD;physical[i].cells[0].flags=DC_CELL_LOADED_TERRAIN;
        attachments[i].record.owner=fr_reactive_cell_owner(physical[i].coord,0);
        if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,8192,700+i,10+i,&attachments[i].record.state,err,cap)) return false;
        channels[i]=(fr_reactive_chunk_t){.count=1,.next_id=13,.attachments=attachments+i};
    }
    body->owner=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={120,3,4,5,0}};
    if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,12,&body->state,err,cap)) return false;
    body->state.coating_oil_fuel_fp=500;body->state.ignition_threshold=.7f;body->state.ignition_integral=.4f;
    *archive=(fr_reactive_archive_t){.next_id=20,.body_count=1,.chunk_count=2,.bodies=body,.chunks=channels};return true;
}
static void test_checkpoint_archive_coordinates_bodies_and_pending_chunks_with_one_counter(void) {
    char err[256]={0};dc_chunk_t *physical=calloc(2,sizeof(*physical));ASSERT_TRUE(physical);
    fr_reactive_record_t body;fr_reactive_chunk_t channels[2];fr_reactive_attachment_t attachments[2];fr_reactive_archive_t input,out={0};
    ASSERT_TRUE(fixture(physical,&input,&body,channels,attachments,err,sizeof(err)));
    uint8_t *bytes=NULL;size_t length=0;ASSERT_TRUE(fr_reactive_archive_encode(&input,physical,2,&bytes,&length,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_archive_decode(bytes,length,physical,2,&out,err,sizeof(err)));
    ASSERT_EQ(out.next_id,20u);ASSERT_EQ(out.body_count,1u);ASSERT_EQ(out.chunk_count,2u);
    ASSERT_TRUE(memcmp(out.bodies,&body,sizeof(body))==0);
    for(uint32_t i=0;i<2;i++) {ASSERT_EQ(out.chunks[i].next_id,20u);ASSERT_TRUE(memcmp(out.chunks[i].attachments,attachments+i,sizeof(*attachments))==0);}
    ASSERT_EQ(channels[0].next_id,13u);fr_reactive_archive_release(&out);free(bytes);free(physical);PASS();
}
static void test_cross_channel_identity_conflicts_and_counter_rollback_are_rejected(void) {
    char err[256]={0};dc_chunk_t *physical=calloc(2,sizeof(*physical));ASSERT_TRUE(physical);
    fr_reactive_record_t body;fr_reactive_chunk_t channels[2];fr_reactive_attachment_t attachments[2];fr_reactive_archive_t input;
    ASSERT_TRUE(fixture(physical,&input,&body,channels,attachments,err,sizeof(err)));
    uint8_t sentinel=0;uint8_t *bytes=&sentinel;size_t length=99;
    for(uint32_t kind=0;kind<3;kind++) {
        ASSERT_TRUE(fixture(physical,&input,&body,channels,attachments,err,sizeof(err)));
        if(kind==0) body.state.parcel_id_low=10;
        if(kind==1) channels[0].next_id=30;
        if(kind==2) body.owner=attachments[0].record.owner;
        ASSERT_TRUE(!fr_reactive_archive_encode(&input,physical,2,&bytes,&length,err,sizeof(err)));
        ASSERT_TRUE(bytes==&sentinel);ASSERT_EQ(length,99u);
    }
    free(physical);PASS();
}
static void test_archive_corruption_and_empty_high_water_are_transactional(void) {
    char err[256]={0};fr_reactive_archive_t input={.next_id=UINT64_C(0x123456789)},out={.next_id=90};
    uint8_t *bytes=NULL;size_t length=0;ASSERT_TRUE(fr_reactive_archive_encode(&input,NULL,0,&bytes,&length,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_archive_decode(bytes,length,NULL,0,&out,err,sizeof(err)));ASSERT_EQ(out.next_id,input.next_id);
    fr_reactive_archive_release(&out);out.next_id=90;
    for(uint32_t kind=0;kind<3;kind++) {
        if(kind==0) bytes[4]^=3;
        if(kind==1) bytes[length-1]^=1;
        ASSERT_TRUE(!fr_reactive_archive_decode(bytes,length-(kind==2),NULL,0,&out,err,sizeof(err)));
        ASSERT_EQ(out.next_id,90u);ASSERT_TRUE(!out.bodies&&!out.chunks);
        if(kind==0) bytes[4]^=3;
        if(kind==1) bytes[length-1]^=1;
    }
    free(bytes);PASS();
}
int main(void) {
    RUN(test_checkpoint_archive_coordinates_bodies_and_pending_chunks_with_one_counter);
    RUN(test_cross_channel_identity_conflicts_and_counter_rollback_are_rejected);
    RUN(test_archive_corruption_and_empty_high_water_are_transactional);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
