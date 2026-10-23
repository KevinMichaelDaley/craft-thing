#define DC_CONTACT_SCALING_NO_MAIN
#include "../../tools/diagnostics/contact_scaling.c"
#undef RUN
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();if(g_fail)return 1;} while(0)

static void test_small_pixel_pairs_do_not_scan_unused_tile_padding(void) {
    for(unsigned shape=0;shape<3;shape++) {
        measure(8,shape,2);
        ASSERT_TRUE(last_emitted>0u);
        ASSERT_TRUE(last_visited<=200u);
        ASSERT_EQ(last_workgroups,1u);
    }
    PASS();
}
static void test_empty_work_list_still_completes_negative_candidates(void) {
    measure(8,0,3);
    ASSERT_EQ(last_emitted,0u);ASSERT_EQ(last_workgroups,0u);ASSERT_EQ(last_rejected,1u);
    PASS();
}
static void test_large_concave_pair_keeps_cooperative_strided_coverage(void) {
    measure(64,1,2);
    ASSERT_TRUE(last_emitted>=64u);ASSERT_EQ(last_workgroups,DC_CONTACT_PIXEL_GROUPS);
    PASS();
}
static void test_disabled_profile_keeps_contacts_without_counter_atomics(void) {
    measure(8,0,2);
    ASSERT_EQ(last_plain_visited,0u);ASSERT_TRUE(last_visited>0u);
    ASSERT_EQ(last_plain_emitted,last_emitted);PASS();
}
int main(void) {
    RUN(test_disabled_profile_keeps_contacts_without_counter_atomics);
    RUN(test_small_pixel_pairs_do_not_scan_unused_tile_padding);
    RUN(test_empty_work_list_still_completes_negative_candidates);
    RUN(test_large_concave_pair_keeps_cooperative_strided_coverage);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
