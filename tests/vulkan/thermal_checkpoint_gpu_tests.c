#define main thermal_checkpoint_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/thermal_gpu.h"

static bool thermal_checkpoint_fixture(dc_gpu_t **g,dc_chunk_t *chunk,char *err,uint32_t cap) {
    chunk->coord=(dc_chunk_coord_t){-5,-7};
    chunk->cells[20*64+20]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[20*64+21].fluid_mass=65536;
    fr_reactive_state_t wood;
    return context(g,err,cap)&&dc_gpu_upload_chunk(*g,0,chunk,err,cap)&&dc_gpu_set_page(*g,0,0,0,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,900,330,91,&wood,err,cap)&&
        fr_gpu_reactive_seed_cell(*g,0,20*64+20,&wood,err,cap)&&fr_thermal_gpu_enable(*g,err,cap)&&
        fr_thermal_gpu_seed_water(*g,0,20*64+21,100,err,cap);
}
static void test_finite_source_snapshot_tracks_real_contact_and_preserves_output_on_failure(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(thermal_checkpoint_fixture(&g,chunk,err,sizeof(err)));
    fr_thermal_probe_t before,after;
    ASSERT_TRUE(fr_thermal_gpu_source_state(g,&before,err,sizeof(err)));
    ASSERT_EQ(before.capacity,200.f);ASSERT_EQ(before.excess_energy,11370.f);
    ASSERT_TRUE(fr_thermal_gpu_probe(g,20,20,true)&&fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_source_state(g,&after,err,sizeof(err)));
    ASSERT_EQ(after.capacity,before.capacity);ASSERT_TRUE(after.excess_energy>0&&after.excess_energy<before.excess_energy);
    fr_thermal_probe_t untouched=after;
    ASSERT_TRUE(!fr_thermal_gpu_source_state(NULL,&after,err,sizeof(err))&&memcmp(&after,&untouched,sizeof(after))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
static void test_checkpoint_restores_exact_parcel_liquid_heat_and_spent_probe_into_new_slot(void) {
    char err[256]={0};dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(thermal_checkpoint_fixture(&g,chunk,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_probe(g,20,20,true));
    for(uint32_t i=0;i<3;i++) ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&before,&found,err,sizeof(err))&&found);
    fr_thermal_probe_t source,restored;
    ASSERT_TRUE(fr_thermal_gpu_source_state(g,&source,err,sizeof(err)));ASSERT_TRUE(source.excess_energy<11370.f);
    float heat[4096],loaded[4096];ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,heat,4096,err,sizeof(err)));
    ASSERT_TRUE(heat[20*64+21]!=100.f&&heat[20*64+21]>0.f);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/thermal_checkpoint.bin",err,sizeof(err)));dc_gpu_destroy(g);g=NULL;
    ASSERT_TRUE(context(&g,err,sizeof(err))&&fr_gpu_load_structure(g,"build/thermal_checkpoint.bin",err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_source_state(g,&restored,err,sizeof(err))&&memcmp(&source,&restored,sizeof(source))==0);
    bool physical_restored=false;
    ASSERT_TRUE(fr_gpu_upload_structure_chunk(g,1,chunk,&physical_restored,err,sizeof(err))&&physical_restored);
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,1,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,1,20*64+20,&after,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&before,&after,sizeof(before))==0);
    ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,1,loaded,4096,err,sizeof(err))&&memcmp(heat,loaded,sizeof(heat))==0);
    /* A held mouse/key is transient input, never a resumed heat source. */
    ASSERT_TRUE(fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_source_state(g,&restored,err,sizeof(err))&&memcmp(&source,&restored,sizeof(source))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));free(chunk);dc_gpu_destroy(g);PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_finite_source_snapshot_tracks_real_contact_and_preserves_output_on_failure);
    RUN(test_checkpoint_restores_exact_parcel_liquid_heat_and_spent_probe_into_new_slot);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
