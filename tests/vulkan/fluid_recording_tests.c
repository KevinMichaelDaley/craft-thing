#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static VkPipeline bound;
static uint32_t mode,seen;
static bool correct;
static uint32_t column_count, row_count;
static VkPipeline correction_pipeline;
static uint32_t correction_groups[10];
void __wrap_vkCmdBindDescriptorSets(VkCommandBuffer c,VkPipelineBindPoint p,VkPipelineLayout l,uint32_t f,uint32_t n,const VkDescriptorSet *s,uint32_t d,const uint32_t *o) {(void)c;(void)p;(void)l;(void)f;(void)n;(void)s;(void)d;(void)o;}
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
void __wrap_vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline v) {(void)c;(void)p;bound=v;}
void __wrap_vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags f,uint32_t o,uint32_t n,const void *v) {
    (void)c;(void)l;(void)f;(void)o;if(n>=12) memcpy(&mode,(const char *)v+8,4);
}
void __wrap_vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {
    (void)c;
    if (correction_pipeline && bound == correction_pipeline && mode < 10u)
        correction_groups[mode] = x;
    if(mode<4&&bound!=(VkPipeline)(uintptr_t)100u) {
        correct=correct&&bound==(VkPipeline)(uintptr_t)(mode+1u)&&x==(mode<2?column_count:row_count)&&y==1&&z==1;
        seen|=1u<<mode;
    }
}
void __wrap_vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *p) {(void)c;(void)p;}
static void test_body_free_transport_is_cooperative(void) {
    dc_gpu_t gpu={.width=640,.height=360,.fluid_step_scale=6.f};
    gpu.projection_pipeline=(VkPipeline)(uintptr_t)100u;
    for(uint32_t i=0;i<4;i++) gpu.fluid_transport_pipelines[i]=(VkPipeline)(uintptr_t)(i+1u);
    column_count=gpu.width;row_count=gpu.height;
    correct=true;seen=0;
    dc_gpu_record_fluid_phase(&gpu,3);
    dc_gpu_record_fluid_phase(&gpu,4);
    ASSERT_EQ(seen,15u);ASSERT_TRUE(correct);PASS();
}
static void test_workspace_dispatches_only_mapped_transport_lines(void) {
    dc_gpu_t gpu = {.width=150, .height=143, .workspace_slots=9, .page_width=3, .fluid_step_scale=1.f};
    gpu.projection_pipeline = (VkPipeline)(uintptr_t)100;
    for (unsigned mode = 0; mode < 4; ++mode)
        gpu.fluid_transport_pipelines[mode] = (VkPipeline)(uintptr_t)(mode+1);
    memset(gpu.slot_page, 0xff, sizeof(gpu.slot_page));
    const uint32_t tiles[] = {UINT32_MAX, 0, 2, 6, 8};
    const uint32_t widths[] = {0, 64, 150, 64, 150};
    const uint32_t heights[] = {0, 64, 64, 143, 143};
    for (unsigned i = 0; i < 5; ++i) {
        gpu.slot_page[5] = tiles[i];
        column_count = widths[i];
        row_count = heights[i];
        correct = true;
        seen = 0;
        dc_gpu_record_fluid_phase(&gpu, 3);
        dc_gpu_record_fluid_phase(&gpu, 4);
        ASSERT_EQ(seen, 15u);
        ASSERT_TRUE(correct);
    }
    PASS();
}

static void check_single_visit_correction(bool seam) {
    dc_gpu_t gpu = {.width=128, .height=64, .workspace_slots=2, .marker_correction=true};
    correction_pipeline = (VkPipeline)(uintptr_t)101u;
    gpu.fluid_pipeline = correction_pipeline;
    memset(correction_groups, 0, sizeof(correction_groups));
    if (seam)
        dc_gpu_record_water_seam_correction(&gpu, gpu.descriptor, correction_pipeline,
                                            128, 64, 2, 1.f, 0, 0, 0);
    else
        dc_gpu_record_fluid_phase(&gpu, 5);
    correction_pipeline = VK_NULL_HANDLE;
    ASSERT_EQ(correction_groups[4], DC_CHUNK_CELLS / 64u);
    ASSERT_EQ(correction_groups[5], DC_CHUNK_CELLS / 64u);
    for (unsigned phase = 6; phase <= 9; ++phase)
        ASSERT_EQ(correction_groups[phase], DC_CHUNK_CELLS / (2u * 64u));
    PASS();
}

static void test_single_visit_resident_correction(void) { check_single_visit_correction(false); }
static void test_single_visit_seam_correction(void) { check_single_visit_correction(true); }

int main(void) {
    RUN(test_body_free_transport_is_cooperative);
    RUN(test_workspace_dispatches_only_mapped_transport_lines);
    RUN(test_single_visit_resident_correction);
    RUN(test_single_visit_seam_correction);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
