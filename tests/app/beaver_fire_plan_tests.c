#include <stdio.h>

#include "../../src/app/beaver_fire_demo.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_massive_rightmost_unlit_pile_and_grounded_player(void) {
    const uint32_t widths[]={480,960},heights[]={270,540};
    for(unsigned k=0;k<2;k++) {
        dc_beaver_fire_plan_t p;ASSERT_TRUE(dc_beaver_fire_plan(widths[k],heights[k],&p));
        ASSERT_EQ(p.count,10);ASSERT_EQ(p.temperature_K,293.15f);
        ASSERT_EQ(p.player_y+40,p.ground);ASSERT_TRUE(p.player_x+80<p.logs[0].x);
        uint32_t area=0,top=p.ground;
        for(unsigned n=0;n<p.count;n++) {
            const dc_beaver_fire_log_t *log=&p.logs[n];
            ASSERT_TRUE(log->x>=p.width*2/3);ASSERT_TRUE(log->x+log->width<p.width);
            ASSERT_TRUE(log->y+log->height<=p.ground);ASSERT_TRUE(log->object);
            if(n) ASSERT_EQ(log->y+log->height,p.logs[n-1].y);
            if(log->y<top) top=log->y;
            area+=log->width*log->height;
            for(unsigned m=0;m<n;m++) ASSERT_TRUE(log->object!=p.logs[m].object);
        }
        ASSERT_TRUE(area>=8000);ASSERT_TRUE(p.ground-top>=p.height/3);
        ASSERT_TRUE(p.logs[0].x+p.logs[0].width>=p.width-16);
    }
    PASS();
}
static void test_invalid_dimensions_do_not_publish_partial_plan(void) {
    dc_beaver_fire_plan_t p={.count=999};
    ASSERT_TRUE(!dc_beaver_fire_plan(100,100,&p));ASSERT_EQ(p.count,999);
    ASSERT_TRUE(!dc_beaver_fire_plan(480,270,NULL));PASS();
}
int main(void) {RUN(test_massive_rightmost_unlit_pile_and_grounded_player);RUN(test_invalid_dimensions_do_not_publish_partial_plan);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
