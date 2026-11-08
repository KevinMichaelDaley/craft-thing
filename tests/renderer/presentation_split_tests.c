#include <stdio.h>
#include "dungeoncraft/presentation.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define PASS() g_pass++
static void test_two_bands_fill_their_halves_exactly(void) {
    dc_presentation_focus_t crops[2]={{0,0,240,270},{240,0,240,270}};
    dc_presentation_layout_t out[2];
    ASSERT_TRUE(dc_presentation_split_layout(480,270,960,540,1,crops,2,out));
    ASSERT_EQ(out[0].left,0);ASSERT_EQ(out[0].top,0);
    ASSERT_EQ(out[0].scale,2);ASSERT_EQ(out[0].width,240);
    ASSERT_EQ(out[1].left,480);ASSERT_EQ(out[1].top,0);
    ASSERT_EQ(out[1].scale,2);
    /* Picks invert through the owning band. */
    uint32_t x,y;
    ASSERT_TRUE(dc_presentation_split_pick(out,2,100,100,&x,&y));ASSERT_EQ(x,50);ASSERT_EQ(y,50);
    ASSERT_TRUE(dc_presentation_split_pick(out,2,700,100,&x,&y));ASSERT_EQ(x,240+(700-480)/2);ASSERT_EQ(y,50);
    ASSERT_TRUE(!dc_presentation_split_pick(out,2,700,0,&x,&y)||x!=240u||(700-480)/2!=110u);
    PASS();
}
static void test_odd_window_letterboxes_vertically(void) {
    dc_presentation_focus_t crops[2]={{0,0,240,270},{240,0,240,270}};
    dc_presentation_layout_t out[2];
    ASSERT_TRUE(dc_presentation_split_layout(480,270,960,541,1,crops,2,out));
    ASSERT_EQ(out[0].scale,2);ASSERT_EQ(out[0].top,0);
    ASSERT_EQ(out[1].left,480);ASSERT_EQ(out[1].top,0);
    PASS();
}
static void test_crop_wider_than_band_fails(void) {
    dc_presentation_focus_t crops[2]={{0,0,240,270},{240,0,240,270}};
    dc_presentation_layout_t out[2];
    ASSERT_TRUE(!dc_presentation_split_layout(480,270,400,300,1,crops,2,out));
    PASS();
}
static void test_out_of_range_points_miss_every_band(void) {
    dc_presentation_focus_t crops[2]={{0,0,240,270},{240,0,240,270}};
    dc_presentation_layout_t out[2];
    ASSERT_TRUE(dc_presentation_split_layout(480,270,960,540,1,crops,2,out));
    uint32_t x,y;
    ASSERT_TRUE(!dc_presentation_split_pick(out,2,960,100,&x,&y));
    ASSERT_TRUE(!dc_presentation_split_pick(out,2,100,540,&x,&y));
    PASS();
}
static void test_single_band_matches_the_focused_layout(void) {
    dc_presentation_focus_t one={120,144,80,96};
    dc_presentation_layout_t split[1],focused;
    ASSERT_TRUE(dc_presentation_split_layout(480,270,1920,1080,2,&one,1,split));
    ASSERT_TRUE(dc_presentation_layout(480,270,1920,1080,2,one,&focused));
    ASSERT_EQ(split[0].left,focused.left);ASSERT_EQ(split[0].top,focused.top);
    ASSERT_EQ(split[0].scale,focused.scale);
    PASS();
}
int main(void) {
    RUN(test_two_bands_fill_their_halves_exactly);
    RUN(test_odd_window_letterboxes_vertically);
    RUN(test_crop_wider_than_band_fails);
    RUN(test_out_of_range_points_miss_every_band);
    RUN(test_single_band_matches_the_focused_layout);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
