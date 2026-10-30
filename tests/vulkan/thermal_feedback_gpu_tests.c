#define main thermal_feedback_fixture_main
#include "reactive_cell_gpu_tests.c"
#undef main
#include <math.h>
#include <sys/stat.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "../../src/vulkan/renderer_internal.h"
#include "../../src/vulkan/thermal_gpu_internal.h"

static bool feedback_fixture(dc_gpu_t **g,fr_reactive_state_t *wood,char *err,uint32_t cap) {
    dc_chunk_t *c=calloc(1,sizeof(*c));if(!c) return false;
    uint32_t bound_water=(uint32_t)llround(39322.*fr_reactive_profile(DC_MATERIAL_WOOD)->moisture_capacity);
    c->cells[20*64+20].material=DC_MATERIAL_WOOD;c->cells[20*64+21].fluid_mass=65536;
    bool okay=context(g,err,cap)&&dc_gpu_upload_chunk(*g,0,c,err,cap)&&dc_gpu_set_page(*g,0,0,0,err,cap)&&
        fr_reactive_state_default(DC_MATERIAL_WOOD,39322,bound_water,350,71,wood,err,cap)&&
        fr_gpu_reactive_seed_cell(*g,0,20*64+20,wood,err,cap)&&fr_thermal_gpu_enable(*g,err,cap)&&
        fr_thermal_gpu_seed_water(*g,0,20*64+21,100,err,cap);
    free(c);return okay;
}
static void test_single_point_inspection_matches_real_solid_and_liquid_inventories(void) {
    char err[256]={0};dc_gpu_t *g=NULL;fr_reactive_state_t wood;
    ASSERT_TRUE(feedback_fixture(&g,&wood,err,sizeof(err)));
    fr_thermal_inspection_t info;
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,20,20,&info,err,sizeof(err)));
    ASSERT_EQ(info.occupied,1u);ASSERT_EQ(info.material,DC_MATERIAL_WOOD);
    printf("Wood hover: %.6fK wetness%.6f energy%.6f capacity%.6f\n",info.temperature_K,info.wetness,info.thermal_energy,info.heat_capacity);
    ASSERT_TRUE(fabsf(info.temperature_K-350.f)<1e-3f&&fabsf(info.wetness-1.f)<1e-4f);
    ASSERT_TRUE(fabsf(info.thermal_energy-wood.thermal_energy)<1e-4f);
    ASSERT_TRUE(fabs(info.heat_capacity-fr_reactive_state_heat_capacity(&wood))<1e-5);
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,21,20,&info,err,sizeof(err)));
    ASSERT_EQ(info.occupied,1u);ASSERT_EQ(info.material,DC_MATERIAL_WATER);
    ASSERT_TRUE(fabsf(info.temperature_K-(293.15f+100.f/4.18f))<1e-3f);ASSERT_EQ(info.wetness,1.f);
    ASSERT_TRUE(fr_thermal_gpu_inspect(g,22,20,&info,err,sizeof(err)));ASSERT_EQ(info.occupied,0u);
    fr_thermal_inspection_t untouched=info;memset(&untouched,0xa5,sizeof(untouched));info=untouched;
    ASSERT_TRUE(!fr_thermal_gpu_inspect(g,640,0,&info,err,sizeof(err))&&memcmp(&info,&untouched,sizeof(info))==0);
    ASSERT_TRUE(present(g,err,sizeof(err)));dc_gpu_destroy(g);PASS();
}
static void test_displayed_hover_text_changes_with_target_without_advancing_physics(void) {
    char err[256]={0};dc_gpu_t *g=NULL;fr_reactive_state_t wood;
    ASSERT_TRUE(feedback_fixture(&g,&wood,err,sizeof(err)));
    uint32_t *first=malloc(480u*270u*4u),*second=malloc(480u*270u*4u);ASSERT_TRUE(first&&second);
    ASSERT_TRUE(fr_thermal_gpu_probe(g,20,20,false)&&present(g,err,sizeof(err))&&dc_gpu_readback(g,first,480u*270u,err,sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_probe(g,21,20,false)&&present(g,err,sizeof(err))&&dc_gpu_readback(g,second,480u*270u,err,sizeof(err)));
    uint32_t white=0,changed=0;
    for(uint32_t y=36;y<48;y++) for(uint32_t x=10;x<170;x++) {white+=first[y*480+x]==0xffffffffu;changed+=first[y*480+x]!=second[y*480+x];}
    ASSERT_TRUE(white>20u&&changed>0u);
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(g,0,20*64+20,&record,&found,err,sizeof(err))&&found);
    ASSERT_TRUE(memcmp(&record.state,&wood,sizeof(wood))==0);
    float energy[4096];ASSERT_TRUE(fr_thermal_gpu_read_water_chunk(g,0,energy,4096,err,sizeof(err)));ASSERT_EQ(energy[20*64+21],100.f);
    mkdir("build/screenshots",0777);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormatFrom(second,480,270,32,480*4,SDL_PIXELFORMAT_ABGR8888);ASSERT_TRUE(image);
    ASSERT_EQ(SDL_SaveBMP(image,"build/screenshots/thermal_hover_water.bmp"),0);SDL_FreeSurface(image);
    free(first);free(second);dc_gpu_destroy(g);PASS();
}
static void test_overlay_reaches_the_far_edge_of_the_view(void) {
    char err[256]={0};dc_gpu_t *g=NULL;
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    uint32_t cell=8u*64u+20u;
    chunk->cells[cell].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(context(&g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,1,chunk,err,sizeof(err))&&
        dc_gpu_set_page(g,0,3,1,err,sizeof(err)));
    fr_reactive_state_t wood;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,350,311,&wood,err,sizeof(err))&&
        fr_gpu_reactive_seed_cell(g,1,cell,&wood,err,sizeof(err))&&
        fr_thermal_gpu_enable(g,err,sizeof(err))&&fr_thermal_gpu_overlay(g,1));
    uint32_t index=200u*g->width+20u;
    ((uint32_t *)g->mapped)[index]=0x12345678u;
    ASSERT_TRUE(fr_renderer_begin(g));
    fr_thermal_record_overlay(g);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    ASSERT_TRUE(fr_renderer_submit(g));
    ASSERT_TRUE(((uint32_t *)g->mapped)[index]!=0x12345678u);
    dc_gpu_destroy(g);free(chunk);PASS();
}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_single_point_inspection_matches_real_solid_and_liquid_inventories);
    RUN(test_displayed_hover_text_changes_with_target_without_advancing_physics);
    RUN(test_overlay_reaches_the_far_edge_of_the_view);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
