#include <stdio.h>

#include "../../src/app/player_clock.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_render_debt_cannot_multiply_into_fifteen_ticks(void) {
    dc_player_clock_plan_t p;
    ASSERT_TRUE(dc_player_clock_plan(.25,1./60,false,&p));ASSERT_EQ(p.ticks,2);
    ASSERT_TRUE(p.remainder>=0&&p.remainder<1./60);
    ASSERT_TRUE(dc_player_clock_plan(0,1./60,true,&p));ASSERT_EQ(p.ticks,1);ASSERT_EQ(p.remainder,0);
    PASS();
}
static void test_normal_cadences_advance_equal_fixed_time(void) {
    const unsigned rates[]={40,60,120};
    for(unsigned r=0;r<3;r++) {
        unsigned ticks=0;double accumulator=0;
        for(unsigned n=0;n<rates[r];n++) {
            dc_player_clock_plan_t p;accumulator+=1./rates[r];
            ASSERT_TRUE(dc_player_clock_plan(accumulator,1./60,false,&p));ticks+=p.ticks;accumulator=p.remainder;
        }
        ASSERT_EQ(ticks,60);
    }
    PASS();
}
int main(void) {RUN(test_render_debt_cannot_multiply_into_fifteen_ticks);RUN(test_normal_cadences_advance_equal_fixed_time);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
