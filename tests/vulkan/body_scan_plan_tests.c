#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/body_scan_gpu.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_scan_recurses_without_fixed_page_limit_and_uses_two_dimensional_dispatch(void) {
    dc_gpu_body_scan_plan_t plan={0};char err[128];
    ASSERT_TRUE(dc_gpu_body_scan_plan_create(UINT32_MAX,65535,65535,&plan,err,sizeof(err)));
    const uint32_t counts[]={UINT32_MAX,33554432u,262144u,2048u,16u};
    ASSERT_EQ(plan.level_count,sizeof(counts)/sizeof(counts[0]));
    ASSERT_EQ(plan.levels[0].groups_x,65535u);ASSERT_EQ(plan.levels[0].groups_y,513u);
    VkDeviceSize end=0;
    for(uint32_t i=0;i<plan.level_count;i++) {
        ASSERT_EQ(plan.levels[i].count,counts[i]);ASSERT_EQ(plan.levels[i].offset,end);
        ASSERT_TRUE((uint64_t)plan.levels[i].groups_x*plan.levels[i].groups_y>=plan.levels[i].groups);
        end+=(VkDeviceSize)counts[i]*2u*sizeof(uint32_t);
    }
    ASSERT_EQ(plan.total_offset,end);ASSERT_EQ(plan.bytes,end+8u);
    dc_gpu_body_scan_plan_destroy(&plan);ASSERT_EQ(plan.levels,NULL);PASS();
}
static void test_scan_boundary_cases_do_not_omit_partial_workgroups(void) {
    const uint32_t counts[]={1,127,128,129,16384,16385};
    const uint32_t levels[]={1,1,1,2,2,3};char err[128];
    for(uint32_t i=0;i<6;i++) {
        dc_gpu_body_scan_plan_t plan={0};
        ASSERT_TRUE(dc_gpu_body_scan_plan_create(counts[i],65535,65535,&plan,err,sizeof(err)));
        ASSERT_EQ(plan.level_count,levels[i]);
        ASSERT_TRUE((uint64_t)plan.levels[0].groups*128u>=counts[i]);
        ASSERT_TRUE((uint64_t)(plan.levels[0].groups-1u)*128u<counts[i]);
        dc_gpu_body_scan_plan_destroy(&plan);
    }PASS();
}
static void test_invalid_or_device_limited_plan_preserves_destination(void) {
    dc_gpu_body_scan_plan_t plan={.bytes=777},before=plan;char err[128]={0};
    ASSERT_TRUE(!dc_gpu_body_scan_plan_create(129,1,1,&plan,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&plan,&before,sizeof(plan))==0);ASSERT_TRUE(err[0]);
    ASSERT_TRUE(!dc_gpu_body_scan_plan_create(0,65535,65535,&plan,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&plan,&before,sizeof(plan))==0);PASS();
}
int main(void) {RUN(test_scan_recurses_without_fixed_page_limit_and_uses_two_dimensional_dispatch);RUN(test_scan_boundary_cases_do_not_omit_partial_workgroups);RUN(test_invalid_or_device_limited_plan_preserves_destination);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
