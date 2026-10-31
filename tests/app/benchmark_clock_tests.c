#include <stdio.h>

#include "../../src/app/benchmark_clock.h"

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_slow_present_does_not_add_a_second_physics_tick(void) {
    const float tick=1.f/60.f;
    ASSERT_EQ(dc_benchmark_step_seconds(.05f,tick),tick);
    PASS();
}

static void test_fast_present_still_runs_one_physics_tick(void) {
    const float tick=1.f/60.f;
    ASSERT_EQ(dc_benchmark_step_seconds(.01f,tick),tick);
    PASS();
}

int main(void) {
    RUN(test_slow_present_does_not_add_a_second_physics_tick);
    RUN(test_fast_present_still_runs_one_physics_tick);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
