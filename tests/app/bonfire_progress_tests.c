#include <stdio.h>
#include "bonfire_progress.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_global_burning_window_can_precede_last_log_ignition(void) {
    bonfire_progress_t p=bonfire_progress_init();
    for(unsigned second=0;second<=20;second++)
        ASSERT_TRUE(!bonfire_progress_observe(&p,second,1000.-second*10.,1u+second%7u,false));
    ASSERT_TRUE(p.sustained);
    ASSERT_TRUE(bonfire_progress_observe(&p,21.,790.,3u,true));PASS();
}
static void test_all_logs_and_full_duration_are_still_required(void) {
    bonfire_progress_t p=bonfire_progress_init();
    for(unsigned second=0;second<20;second++)
        ASSERT_TRUE(!bonfire_progress_observe(&p,second,1000.-second*10.,2u,true));
    ASSERT_TRUE(bonfire_progress_observe(&p,20.,800.,2u,true));PASS();
}
static void test_extinguished_or_unfunded_fire_cannot_pass(void) {
    bonfire_progress_t p=bonfire_progress_init();
    for(unsigned second=0;second<20;second++)
        bonfire_progress_observe(&p,second,1000.-second*10.,2u,true);
    ASSERT_TRUE(!bonfire_progress_observe(&p,20.,810.,2u,true));
    p=bonfire_progress_init();
    for(unsigned second=0;second<20;second++)
        bonfire_progress_observe(&p,second,1000.-second*10.,2u,true);
    ASSERT_TRUE(!bonfire_progress_observe(&p,20.,800.,0u,true));PASS();
}
int main(void) {
    RUN(test_global_burning_window_can_precede_last_log_ignition);
    RUN(test_all_logs_and_full_duration_are_still_required);
    RUN(test_extinguished_or_unfunded_fire_cannot_pass);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
