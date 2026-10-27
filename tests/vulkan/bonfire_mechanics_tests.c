#define main bonfire_mechanics_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include "../../src/app/bonfire.h"
#include "../../src/vulkan/reactive_storage_gpu.h"
static void test_constructed_pit_supports_actual_independent_wood_bodies(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,640,448,"build/shaders/pattern.comp.spv",err,sizeof(err))&&dc_gpu_set_body_origin(g,(dc_chunk_coord_t){-1,-3},err,sizeof(err)));
    dc_chunk_coord_t coords[70];for(uint32_t i=0;i<70;i++) coords[i]=(dc_chunk_coord_t){(int64_t)(i%10)-1,(int64_t)(i/10)-3};
    dc_thermal_bench_chunk_t *chunks=calloc(70,sizeof(*chunks));ASSERT_TRUE(chunks&&dc_bonfire_build(coords,70,chunks,err,sizeof(err)));
    for(uint32_t i=0;i<70;i++) ASSERT_TRUE(dc_gpu_upload_chunk(g,i,chunks[i].chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,i%10,i/10,i,err,sizeof(err))&&fr_gpu_reactive_import_chunk(g,i,chunks[i].chunk,&chunks[i].reactive,err,sizeof(err)));
    ASSERT_TRUE(dc_bonfire_admit(g,err,sizeof(err)));
    dc_gpu_extraction_stats_t stats;ASSERT_TRUE(dc_gpu_read_extraction_stats(g,&stats));ASSERT_EQ(stats.owned_bodies,26);
    fr_reactive_record_t *records=NULL;uint32_t count=0,hot=0,wet=0;uint64_t next=0;
    ASSERT_TRUE(fr_gpu_reactive_export_bodies(g,&records,&count,&next,err,sizeof(err)));ASSERT_EQ(count,246);
    for(uint32_t i=0;i<count;i++) {
        float temperature=0;ASSERT_TRUE(records[i].owner.kind==FR_REACTIVE_OWNER_BODY&&fr_reactive_state_temperature(&records[i].state,&temperature,err,sizeof(err)));
        ASSERT_EQ(records[i].state.flags&FR_REACTIVE_FLAMING,0);
        if(temperature>1000) hot++;
        if(records[i].state.bound_water_fp) wet++;
    }
    ASSERT_EQ(hot,6);ASSERT_EQ(wet,120);free(records);
    dc_chunk_t *physical=calloc(1,sizeof(*physical));ASSERT_TRUE(physical&&dc_gpu_download_chunk(g,43,physical,err,sizeof(err)));
    for(uint32_t i=0;i<4096;i++) ASSERT_TRUE(physical->cells[i].material!=DC_MATERIAL_WOOD);
    free(physical);dc_thermal_bench_release(chunks,70);dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_constructed_pit_supports_actual_independent_wood_bodies);printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
