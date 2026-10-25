#define main thermal_admission_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include "ferrum/fire/thermal_gpu.h"
#include "../../src/vulkan/reactive_storage_gpu.h"

static bool empty_bench(dc_gpu_t **g,char *err,uint32_t cap) {
    dc_chunk_t *c=calloc(1,sizeof(*c));if(!c) return false;
    c->coord=(dc_chunk_coord_t){-7,12};
    bool okay=context(g,err,cap)&&dc_gpu_upload_chunk(*g,0,c,err,cap)&&
        dc_gpu_set_page(*g,0,0,0,err,cap)&&fr_thermal_gpu_enable(*g,err,cap);
    free(c);return okay;
}
static void test_new_wood_uses_canonical_ambient_state_and_accepts_finite_heat(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(empty_bench(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_WOOD,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&record,&found,err,sizeof(err))&&found);
    uint64_t id=((uint64_t)record.state.parcel_id_high<<32)|record.state.parcel_id_low;
    fr_reactive_state_t expected;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,FR_THERMAL_AMBIENT_K,id,&expected,err,sizeof(err)));
    ASSERT_TRUE(fabsf(record.state.thermal_energy-expected.thermal_energy)<1e-4f);
    fr_reactive_state_t payload=record.state;payload.thermal_energy=expected.thermal_energy;
    ASSERT_TRUE(memcmp(&payload,&expected,sizeof(expected))==0);
    fr_reactive_owner_t owner=fr_reactive_cell_owner((dc_chunk_coord_t){-7,12},20*64+20);
    ASSERT_TRUE(memcmp(&record.owner,&owner,sizeof(owner))==0);
    fr_thermal_sample_t sample,heated;fr_thermal_probe_t probe={.excess_energy=11370,.capacity=200},after;
    ASSERT_TRUE(fr_thermal_sample_from_state(&expected,&sample,err,sizeof(err))&&
        fr_thermal_probe_exchange(probe,sample,1.f/60.f,&after,&heated,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_probe(g,20,20,true)&&fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(record.state.thermal_energy>expected.thermal_energy);
    ASSERT_TRUE(fabsf(record.state.thermal_energy-heated.energy)<1e-4f);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
static void test_replacement_stone_gets_a_new_identity_without_old_wood_heat(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(empty_bench(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_WOOD,err,sizeof(err)));
    fr_reactive_record_t old,record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&old,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(fr_thermal_gpu_probe(g,20,20,true)&&fr_thermal_gpu_step(g,1.f/60.f,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,20,20,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&record,&found,err,sizeof(err))&&found);
    uint64_t old_id=((uint64_t)old.state.parcel_id_high<<32)|old.state.parcel_id_low;
    uint64_t id=((uint64_t)record.state.parcel_id_high<<32)|record.state.parcel_id_low;
    ASSERT_TRUE(id>old_id);
    fr_reactive_state_t expected;ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_STONE,176947,0,FR_THERMAL_AMBIENT_K,id,&expected,err,sizeof(err)));
    ASSERT_TRUE(fabsf(record.state.thermal_energy-expected.thermal_energy)<1e-4f);
    fr_reactive_state_t payload=record.state;payload.thermal_energy=expected.thermal_energy;
    ASSERT_TRUE(memcmp(&payload,&expected,sizeof(expected))==0);
    fr_reactive_arena_stats_t stats;ASSERT_TRUE(fr_reactive_arena_stats(g,&g->reactive->arena,&stats));ASSERT_EQ(stats.live,1u);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
static void test_brush_admission_grows_and_reuses_storage_without_a_parcel_quota(void) {
    char err[256]={0};dc_gpu_t *g=NULL;ASSERT_TRUE(empty_bench(&g,err,sizeof(err)));
    uint32_t expected=0;for(int y=-14;y<=14;y++) for(int x=-14;x<=14;x++) expected+=x*x+y*y<=14*14;
    uint64_t prior=0;
    for(uint32_t cycle=0;cycle<5;cycle++) {
        ASSERT_TRUE(dc_gpu_paint_material(g,20,20,14,DC_MATERIAL_WOOD,err,sizeof(err)));
        fr_reactive_arena_stats_t stats;ASSERT_TRUE(fr_reactive_arena_stats(g,&g->reactive->arena,&stats));
        ASSERT_EQ(stats.live,expected);ASSERT_TRUE(stats.capacity>=expected&&stats.capacity>64u);
        fr_reactive_record_t record;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&record,&found,err,sizeof(err))&&found);
        uint64_t id=((uint64_t)record.state.parcel_id_high<<32)|record.state.parcel_id_low;
        ASSERT_TRUE(id>prior);prior=id;
        ASSERT_TRUE(dc_gpu_paint_material(g,20,20,14,DC_MATERIAL_AIR,err,sizeof(err)));
        ASSERT_TRUE(fr_reactive_arena_stats(g,&g->reactive->arena,&stats));ASSERT_EQ(stats.live,0u);ASSERT_EQ(stats.free,stats.capacity);
    }
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_new_wood_uses_canonical_ambient_state_and_accepts_finite_heat);
    RUN(test_replacement_stone_gets_a_new_identity_without_old_wood_heat);
    RUN(test_brush_admission_grows_and_reuses_storage_without_a_parcel_quota);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
