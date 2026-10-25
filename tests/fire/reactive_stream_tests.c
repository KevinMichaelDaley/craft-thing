#define main existing_stream_fixture_main
#include "../world/stream_tests.c"
#undef main

#include <string.h>
#include "ferrum/fire/reactive_chunk.h"

static bool reactive_input(dc_chunk_t *c,fr_reactive_attachment_t *a,char *err,uint32_t cap) {
    c->coord=(dc_chunk_coord_t){-5,-7};c->cells[0].material=DC_MATERIAL_WOOD;c->cells[0].flags=DC_CELL_LOADED_TERRAIN;
    a->local_index=0;a->record.owner=fr_reactive_cell_owner(c->coord,0);
    if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,8192,800,50,&a->record.state,err,cap)) return false;
    a->record.state.coating_oil_fuel_fp=300;a->record.state.ignition_threshold=.7f;a->record.state.ignition_integral=.4f;
    return true;
}
static void test_worker_atomically_preserves_physics_colors_and_immutable_reaction_input(void) {
    char directory[]="build/reactive_stream_XXXXXX",err[256];ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);
    dc_chunk_t *c=calloc(1,sizeof(*c));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(c&&colors);
    fr_reactive_attachment_t a={0};ASSERT_TRUE(reactive_input(c,&a,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=51,.attachments=&a};fr_reactive_record_t saved=a.record;
    colors->terrain[0][0]=.3f;colors->terrain[0][3]=1.f;
    ASSERT_TRUE(dc_stream_request_save_reactive(s,c,colors,&channel,7));
    a.record.state.thermal_energy=99;colors->terrain[0][0]=.9f;c->cells[0].material=DC_MATERIAL_STONE;
    dc_stream_result_t result={0};ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,8));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);
    ASSERT_EQ(result.chunk->cells[0].material,DC_MATERIAL_WOOD);ASSERT_EQ(result.colors->terrain[0][0],.3f);
    ASSERT_TRUE(result.reactive);ASSERT_EQ(result.reactive->count,1u);ASSERT_EQ(result.reactive->next_id,51u);
    ASSERT_TRUE(memcmp(&result.reactive->attachments[0].record,&saved,sizeof(saved))==0);
    dc_stream_result_release(&result);dc_stream_destroy(s);free(c);free(colors);PASS();
}
static void test_invalid_channel_cannot_overwrite_existing_chunk_and_corrupt_channel_fails_load(void) {
    char directory[]="build/reactive_stream_bad_XXXXXX",err[256];ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);
    dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);fr_reactive_attachment_t a={0};ASSERT_TRUE(reactive_input(c,&a,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=51,.attachments=&a};dc_stream_result_t result={0};
    ASSERT_TRUE(dc_stream_request_save_reactive(s,c,NULL,&channel,7));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    a.record.owner.key[0]++;ASSERT_TRUE(!dc_stream_request_save_reactive(s,c,NULL,&channel,8));
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,9));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);dc_stream_result_release(&result);
    char path[256];snprintf(path,sizeof(path),"%s/chunk_%016llx_%016llx.bin",directory,(unsigned long long)c->coord.x,(unsigned long long)c->coord.y);
    FILE *file=fopen(path,"r+b");ASSERT_TRUE(file);ASSERT_EQ(fseek(file,-1,SEEK_END),0);int value=fgetc(file);ASSERT_TRUE(value!=EOF);
    ASSERT_EQ(fseek(file,-1,SEEK_END),0);ASSERT_TRUE(fputc(value^1,file)!=EOF);ASSERT_EQ(fclose(file),0);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,10));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_FAILED);ASSERT_TRUE(!result.chunk);
    dc_stream_result_release(&result);dc_stream_destroy(s);free(c);PASS();
}
static void test_empty_channel_retains_high_water_and_old_chunks_remain_ambient(void) {
    char directory[]="build/reactive_stream_empty_XXXXXX";ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_chunk_t channel={.next_id=UINT64_C(0x123456789)};dc_stream_result_t result={0};
    ASSERT_TRUE(dc_stream_request_save_reactive(s,c,NULL,&channel,7));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,8));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);
    ASSERT_TRUE(result.reactive);ASSERT_EQ(result.reactive->count,0u);ASSERT_EQ(result.reactive->next_id,channel.next_id);dc_stream_result_release(&result);
    c->coord.x=1;ASSERT_TRUE(dc_stream_request_save(s,c,9));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,10));ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(!result.reactive);
    dc_stream_result_release(&result);dc_stream_destroy(s);free(c);PASS();
}
int main(void) {
    RUN(test_worker_atomically_preserves_physics_colors_and_immutable_reaction_input);
    RUN(test_invalid_channel_cannot_overwrite_existing_chunk_and_corrupt_channel_fails_load);
    RUN(test_empty_channel_retains_high_water_and_old_chunks_remain_ambient);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
