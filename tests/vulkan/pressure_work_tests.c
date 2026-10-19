#define _POSIX_C_SOURCE 200809L
#define main fluid_reference_suite_main
#include "fluid_tests.c"
#undef main

static void test_pressure_work_is_empty_then_wakes_for_local_water(void) {
    dc_chunk_t left={0},right={0};char err[256]={0};
    dc_gpu_t *gpu=make_grid(&left,&right,err,sizeof(err));
    ASSERT_TRUE(gpu);
    bool available=gpu->pressure_work_mapped!=NULL;
    if(!available) {dc_gpu_destroy(gpu);ASSERT_TRUE(available);}
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    const uint32_t *work=gpu->pressure_work_mapped;
    ASSERT_EQ(work[0],0u);ASSERT_EQ(work[1],0u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu,110,20,1,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    ASSERT_TRUE(work[0]>0u&&work[0]<32u);
    ASSERT_EQ(work[1],work[0]);ASSERT_EQ(work[2],1u);ASSERT_EQ(work[3],1u);
    dc_gpu_destroy(gpu);PASS();
}
int main(void) {
    RUN(test_pressure_work_is_empty_then_wakes_for_local_water);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
