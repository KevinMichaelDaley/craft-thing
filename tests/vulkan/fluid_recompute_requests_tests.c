#define DC_COLOR_SYNC_PROFILE 1
#define main partial_request_fixture_main
#include "fluid_transport_partial_tests.c"
#undef main

static void test_recomputed_requests_preserve_exact_water_and_pigment(void) {
    for(unsigned mode=0;mode<4;mode++) for(unsigned mixed=0;mixed<2;mixed++) {
        g_mixed_reach=mixed!=0;
        g_profile_shader="build/diagnostics/fluid_transport_profile_16.comp.spv";
        partial_case(mode);uint64_t expected=g_color_hash;
        g_profile_shader="build/diagnostics/fluid_transport_recompute_profile.comp.spv";
        partial_case(mode);ASSERT_EQ(g_color_hash,expected);
    }
    PASS();
}
int main(void) {RUN(test_recomputed_requests_preserve_exact_water_and_pigment);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
