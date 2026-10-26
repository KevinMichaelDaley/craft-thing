#define main existing_stream_main
#include "../world/stream_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/reactive_chunk.h"

static bool thermal_input(dc_chunk_t *c,fr_reactive_attachment_t *a,char *err,uint32_t cap) {
    c->coord=(dc_chunk_coord_t){-5,-7};c->cells[0].material=DC_MATERIAL_WOOD;c->cells[0].flags=DC_CELL_LOADED_TERRAIN;
    a->local_index=0;a->record.owner=fr_reactive_cell_owner(c->coord,0);
    return fr_reactive_state_default(DC_MATERIAL_WOOD,39322,8192,350,50,&a->record.state,err,cap);
}

static void test_worker_preserves_immutable_liquid_enthalpy_with_colors_and_parcels(void) {
    char directory[]="build/thermal_stream_XXXXXX",err[256];ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);
    dc_chunk_t *c=calloc(1,sizeof(*c));dc_chunk_colors_t *colors=calloc(1,sizeof(*colors));ASSERT_TRUE(c&&colors);
    fr_reactive_attachment_t a={0};ASSERT_TRUE(thermal_input(c,&a,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=51,.attachments=&a};fr_reactive_record_t saved=a.record;
    float heat[4096]={0};c->cells[1].fluid_mass=65536;c->cells[2].fluid_mass=1024;
    heat[1]=123.375f;heat[2]=-2.25f;colors->water[1][0]=.6f;colors->water[1][3]=1.f;
    ASSERT_TRUE(dc_stream_request_save_thermal(s,c,colors,&channel,heat,7));
    heat[1]=99;c->cells[1].fluid_mass=0;a.record.state.thermal_energy=99;colors->water[1][0]=.9f;
    dc_stream_result_t result={0};ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,8)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);
    ASSERT_TRUE(result.water_excess&&result.reactive);ASSERT_EQ(result.water_excess[1],123.375f);ASSERT_EQ(result.water_excess[2],-2.25f);
    ASSERT_EQ(result.chunk->cells[1].fluid_mass,65536u);ASSERT_EQ(result.colors->water[1][0],.6f);
    ASSERT_TRUE(memcmp(&result.reactive->attachments[0].record,&saved,sizeof(saved))==0);
    for(uint32_t i=3;i<4096;i++) ASSERT_EQ(result.water_excess[i],0.f);
    dc_stream_result_release(&result);ASSERT_TRUE(!result.water_excess);dc_stream_destroy(s);free(c);free(colors);PASS();
}
static void test_invalid_liquid_cannot_replace_a_valid_streamed_chunk(void) {
    char directory[]="build/thermal_stream_bad_XXXXXX",err[256];ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_attachment_t a={0};ASSERT_TRUE(thermal_input(c,&a,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=51,.attachments=&a};float heat[4096]={0};
    c->cells[1].fluid_mass=65536;heat[1]=100;
    ASSERT_TRUE(dc_stream_request_save_thermal(s,c,NULL,&channel,heat,7));
    dc_stream_result_t result={0};ASSERT_TRUE(wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    heat[2]=1;ASSERT_TRUE(!dc_stream_request_save_thermal(s,c,NULL,&channel,heat,8));heat[2]=0;
    heat[1]=NAN;ASSERT_TRUE(!dc_stream_request_save_thermal(s,c,NULL,&channel,heat,9));
    heat[1]=-10000;ASSERT_TRUE(!dc_stream_request_save_thermal(s,c,NULL,&channel,heat,10));
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,11)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);
    ASSERT_TRUE(result.water_excess);ASSERT_EQ(result.water_excess[1],100.f);
    dc_stream_result_release(&result);dc_stream_destroy(s);free(c);PASS();
}
static void test_old_chunks_have_ambient_liquid_and_future_or_corrupt_chunks_fail(void) {
    char directory[]="build/thermal_stream_version_XXXXXX",err[256];ASSERT_TRUE(mkdtemp(directory));
    dc_streamer_t *s=dc_stream_create(directory,123,4);ASSERT_TRUE(s);dc_chunk_t *c=calloc(1,sizeof(*c));ASSERT_TRUE(c);
    fr_reactive_attachment_t a={0};ASSERT_TRUE(thermal_input(c,&a,err,sizeof(err)));
    fr_reactive_chunk_t channel={.count=1,.next_id=51,.attachments=&a};dc_stream_result_t result={0};
    ASSERT_TRUE(dc_stream_request_save_reactive(s,c,NULL,&channel,7)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,8)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_LOADED);ASSERT_TRUE(!result.water_excess);dc_stream_result_release(&result);
    float heat[4096]={0};c->cells[1].fluid_mass=65536;heat[1]=100;
    ASSERT_TRUE(dc_stream_request_save_thermal(s,c,NULL,&channel,heat,9)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    char path[256];snprintf(path,sizeof(path),"%s/chunk_%016llx_%016llx.bin",directory,(unsigned long long)c->coord.x,(unsigned long long)c->coord.y);
    FILE *file=fopen(path,"r+b");ASSERT_TRUE(file);ASSERT_EQ(fseek(file,-1,SEEK_END),0);int value=fgetc(file);ASSERT_TRUE(value!=EOF);
    ASSERT_EQ(fseek(file,-1,SEEK_END),0);ASSERT_TRUE(fputc(value^1,file)!=EOF);ASSERT_EQ(fclose(file),0);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,10)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_FAILED);dc_stream_result_release(&result);
    ASSERT_TRUE(dc_stream_request_save_thermal(s,c,NULL,&channel,heat,11)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_SAVED);dc_stream_result_release(&result);
    file=fopen(path,"r+b");ASSERT_TRUE(file);uint32_t future=10;ASSERT_EQ(fseek(file,4,SEEK_SET),0);ASSERT_EQ(fwrite(&future,4,1,file),1u);ASSERT_EQ(fclose(file),0);
    ASSERT_TRUE(dc_stream_request_load(s,c->coord,12)&&wait_result(s,&result));ASSERT_EQ(result.kind,DC_STREAM_FAILED);dc_stream_result_release(&result);
    dc_stream_destroy(s);free(c);PASS();
}
int main(void) {
    RUN(test_worker_preserves_immutable_liquid_enthalpy_with_colors_and_parcels);
    RUN(test_invalid_liquid_cannot_replace_a_valid_streamed_chunk);
    RUN(test_old_chunks_have_ambient_liquid_and_future_or_corrupt_chunks_fail);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
