#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/reactive_chunk.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static bool fixture(dc_chunk_t *c,fr_reactive_attachment_t a[3],char *err,uint32_t cap) {
    memset(c,0,sizeof(*c));memset(a,0,sizeof(*a)*3u);c->coord=(dc_chunk_coord_t){-5,-7};
    c->cells[0].material=DC_MATERIAL_WOOD;c->cells[0].flags=DC_CELL_LOADED_TERRAIN;
    a[0].local_index=0;a[0].record.owner=fr_reactive_cell_owner(c->coord,0);
    if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,800,100,&a[0].record.state,err,cap)) return false;
    for(uint32_t i=1;i<3;i++) {
        uint32_t row=10+i;c->particles[row]=(dc_mpm_particle_t){.id_lo=11,.id_hi=22,
            .mass_fp=1234,.material=DC_MATERIAL_LEAF_DEBRIS};
        a[i].local_index=row;
        a[i].record.owner=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_PARTICLE,.key={11,22,100+i,2,0}};
        if(!fr_reactive_state_from_particle(c->particles+row,600+i,100+i,&a[i].record.state,err,cap)) return false;
        a[i].record.state.coating_oil_fuel_fp=300+i;
    }
    c->particle_count=2;return true;
}
static void test_negative_chunk_binds_duplicate_pids_to_distinct_saved_rows(void) {
    char err[256]={0};dc_chunk_t *c=malloc(sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_attachment_t a[3];ASSERT_TRUE(fixture(c,a,err,sizeof(err)));
    fr_reactive_chunk_t input={.count=3,.next_id=103,.attachments=a},out={0};
    size_t size=0,written=0;ASSERT_TRUE(fr_reactive_chunk_size(3,&size));uint8_t *bytes=malloc(size);ASSERT_TRUE(bytes);
    ASSERT_TRUE(fr_reactive_chunk_encode(&input,c,bytes,size,&written,err,sizeof(err)));ASSERT_EQ(written,size);
    ASSERT_TRUE(fr_reactive_chunk_decode(bytes,size,c,&out,err,sizeof(err)));ASSERT_EQ(out.count,3u);ASSERT_EQ(out.next_id,103u);
    for(uint32_t i=0;i<3;i++) {
        ASSERT_EQ(out.attachments[i].local_index,a[i].local_index);
        ASSERT_TRUE(memcmp(&out.attachments[i].record,&a[i].record,sizeof(a[i].record))==0);
    }
    fr_reactive_chunk_release(&out);free(bytes);free(c);PASS();
}
static void test_incompatible_physical_bindings_and_duplicate_rows_are_rejected(void) {
    char err[256]={0};dc_chunk_t *c=malloc(sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_attachment_t a[3];ASSERT_TRUE(fixture(c,a,err,sizeof(err)));
    fr_reactive_chunk_t input={.count=3,.next_id=103,.attachments=a};
    ASSERT_TRUE(fr_reactive_chunk_valid(&input,c,err,sizeof(err)));
    for(uint32_t kind=0;kind<6;kind++) {
        ASSERT_TRUE(fixture(c,a,err,sizeof(err)));
        if(kind==0) a[0].record.owner.key[0]++;
        if(kind==1) c->cells[0].material=DC_MATERIAL_STONE;
        if(kind==2) c->particles[11].id_lo++;
        if(kind==3) c->particles[11].mass_fp=0;
        if(kind==4) a[2].local_index=11;
        if(kind==5) {c->cells[11].material=DC_MATERIAL_LEAF_DEBRIS;c->cells[11].flags=DC_CELL_LOADED_TERRAIN;}
        ASSERT_TRUE(!fr_reactive_chunk_valid(&input,c,err,sizeof(err)));
    }
    free(c);PASS();
}
static void test_corrupt_newer_or_truncated_channels_cannot_replace_output(void) {
    char err[256]={0};dc_chunk_t *c=malloc(sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_attachment_t a[3];ASSERT_TRUE(fixture(c,a,err,sizeof(err)));
    fr_reactive_chunk_t input={.count=3,.next_id=103,.attachments=a},out={.count=91,.next_id=92};
    size_t size=0,written=0;ASSERT_TRUE(fr_reactive_chunk_size(3,&size));uint8_t *bytes=malloc(size),*saved=malloc(size);ASSERT_TRUE(bytes&&saved);
    ASSERT_TRUE(fr_reactive_chunk_encode(&input,c,bytes,size,&written,err,sizeof(err)));memcpy(saved,bytes,size);
    for(uint32_t kind=0;kind<4;kind++) {
        memcpy(bytes,saved,size);
        if(kind==0) bytes[4]=2;
        if(kind==1) bytes[32]^=1;
        if(kind==2) bytes[size-1]^=1;
        ASSERT_TRUE(!fr_reactive_chunk_decode(bytes,size-(kind==3),c,&out,err,sizeof(err)));
        ASSERT_EQ(out.count,91u);ASSERT_EQ(out.next_id,92u);ASSERT_TRUE(!out.attachments);
    }
    memset(bytes,0xa5,size);memcpy(saved,bytes,size);written=99;
    ASSERT_TRUE(!fr_reactive_chunk_encode(&input,c,bytes,size-1,&written,err,sizeof(err)));
    ASSERT_EQ(written,99u);ASSERT_TRUE(memcmp(bytes,saved,size)==0);
    free(bytes);free(saved);free(c);PASS();
}
int main(void) {
    RUN(test_negative_chunk_binds_duplicate_pids_to_distinct_saved_rows);
    RUN(test_incompatible_physical_bindings_and_duplicate_rows_are_rejected);
    RUN(test_corrupt_newer_or_truncated_channels_cannot_replace_output);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
