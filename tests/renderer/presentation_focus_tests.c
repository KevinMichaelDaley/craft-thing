#include <stdio.h>
#include "dungeoncraft/presentation.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_focus_enlarges_without_losing_grid_coordinates(void) {
    dc_presentation_layout_t r;uint32_t x,y;
    ASSERT_TRUE(dc_presentation_layout(480,270,1920,1080,2,(dc_presentation_focus_t){120,144,80,96},&r));
    ASSERT_EQ(r.scale,11);ASSERT_EQ(r.left,520);ASSERT_EQ(r.top,12);
    ASSERT_TRUE(dc_presentation_pick(r,r.left+40*11,r.top+75*11,&x,&y));ASSERT_EQ(x,160);ASSERT_EQ(y,219);
    ASSERT_TRUE(!dc_presentation_pick(r,r.left-1,r.top,&x,&y));PASS();
}
static void test_overview_retains_original_zoom(void) {
    dc_presentation_layout_t r;ASSERT_TRUE(dc_presentation_layout(480,270,1920,1080,2,(dc_presentation_focus_t){0},&r));
    ASSERT_EQ(r.width,480);ASSERT_EQ(r.height,270);ASSERT_EQ(r.scale,2);PASS();
}
static void test_invalid_crop_does_not_publish_a_layout(void) {
    dc_presentation_layout_t r={.scale=7};ASSERT_TRUE(!dc_presentation_layout(480,270,1920,1080,2,(dc_presentation_focus_t){470,0,80,96},&r));
    ASSERT_EQ(r.scale,7);PASS();
}
int main(void) {RUN(test_focus_enlarges_without_losing_grid_coordinates);RUN(test_overview_retains_original_zoom);RUN(test_invalid_crop_does_not_publish_a_layout);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
