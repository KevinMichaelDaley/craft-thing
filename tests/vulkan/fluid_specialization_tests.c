#define _POSIX_C_SOURCE 200809L
#define main fluid_reference_suite_main
#include "fluid_tests.c"
#undef main

static void test_production_phases_preserve_generic_fluid_recurrence(void) {
    dc_chunk_t left = {0},right = {0};
    for (uint32_t y=0;y<64;y++) for (uint32_t x=0;x<64;x++) {
        left.cells[y*64+x].material=x==0||y==63?DC_MATERIAL_STONE:DC_MATERIAL_AIR;
        right.cells[y*64+x].material=x==63||y==63?DC_MATERIAL_STONE:DC_MATERIAL_AIR;
        if(y>=28&&y<60) {
            left.cells[y*64+x].fluid_mass=x>3&&x<63?DC_FLUID_FULL-(x*73+y*97)%8192:0;
            right.cells[y*64+x].fluid_mass=x<60?DC_FLUID_FULL-(x*101+y*41)%8192:0;
        }
    }
    char err[256]={0};
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    dc_gpu_t *production=make_grid(&left,&right,err,sizeof(err));
    ASSERT_TRUE(production);
    bool specialized=true;
    for(uint32_t phase=0;phase<6;phase++) if(phase!=4)
        specialized &= production->projection_phase_pipelines[phase]!=VK_NULL_HANDLE;
    if(!specialized) {dc_gpu_destroy(production);ASSERT_TRUE(specialized);}
    ASSERT_EQ(setenv("DC_FLUID_VARIANTS","generic",1),0);
    dc_gpu_t *reference=make_grid(&left,&right,err,sizeof(err));
    ASSERT_EQ(unsetenv("DC_FLUID_VARIANTS"),0);
    ASSERT_TRUE(reference);
    for(uint32_t tick=0;tick<3;tick++) {
        ASSERT_TRUE(dc_gpu_fluid_step(production,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_fluid_step(reference,err,sizeof(err)));
        for(uint32_t slot=0;slot<2;slot++) {
            dc_chunk_t a,b;
            ASSERT_TRUE(dc_gpu_download_chunk(production,slot,&a,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_download_chunk(reference,slot,&b,err,sizeof(err)));
            ASSERT_EQ(memcmp(a.cells,b.cells,sizeof(a.cells)),0);
            ASSERT_EQ(memcmp(a.face_velocity,b.face_velocity,sizeof(a.face_velocity)),0);
        }
    }
    dc_gpu_destroy(production);dc_gpu_destroy(reference);PASS();
}
int main(void) {
    RUN(test_production_phases_preserve_generic_fluid_recurrence);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
