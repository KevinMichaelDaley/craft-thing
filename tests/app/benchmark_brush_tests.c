#include <stdio.h>
#include <string.h>
#include "../../src/app/benchmark_brush.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_visible_brush_coordinates_and_bounds(void) {
    dc_benchmark_brush_t brush;
    ASSERT_TRUE(dc_benchmark_brush_parse("140,188,3",480,270,&brush));
    ASSERT_EQ(brush.x,140u);ASSERT_EQ(brush.y,188u);ASSERT_EQ(brush.radius,3u);
    ASSERT_TRUE(dc_benchmark_brush_parse("479,269,32",480,270,&brush));
    ASSERT_TRUE(dc_benchmark_brush_parse("0,0,0",480,270,&brush));PASS();
}
static void test_bad_brush_input_does_not_change_pending_command(void) {
    const char *bad[]={"","-1,2,3","+1,2,3","480,0,0","0,270,0","1,2,33","1,2,3junk","1,2","1,2,3,4","4294967296,0,0","999999999999999999999999,0,0"};
    dc_benchmark_brush_t before={7,8,9},brush=before;
    for(uint32_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        ASSERT_TRUE(!dc_benchmark_brush_parse(bad[i],480,270,&brush));ASSERT_TRUE(memcmp(&brush,&before,sizeof(brush))==0);
    }
    ASSERT_TRUE(!dc_benchmark_brush_parse(NULL,480,270,&brush));
    ASSERT_TRUE(!dc_benchmark_brush_parse("1,2,3",0,270,&brush));
    ASSERT_TRUE(!dc_benchmark_brush_parse("1,2,3",480,270,NULL));PASS();
}
static void test_finite_pour_is_one_real_event_after_warmup_while_erosion_repeats(void) {
    uint32_t pours=0,erosions=0;
    for(uint32_t frame=0;frame<1008;frame++) {
        bool once=dc_benchmark_brush_due(false,frame),repeat=dc_benchmark_brush_due(true,frame);
        if(frame<8u) ASSERT_TRUE(!once&&!repeat);
        if(frame==8u) ASSERT_TRUE(once&&repeat);
        if(frame>8u) ASSERT_TRUE(!once&&repeat);
        pours+=once;erosions+=repeat;
    }
    ASSERT_EQ(pours,1u);ASSERT_EQ(erosions,1000u);PASS();
}
int main(void) {RUN(test_visible_brush_coordinates_and_bounds);RUN(test_bad_brush_input_does_not_change_pending_command);RUN(test_finite_pour_is_one_real_event_after_warmup_while_erosion_repeats);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
