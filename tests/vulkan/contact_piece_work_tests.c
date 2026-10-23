#define DC_CONTACT_SCALING_NO_MAIN
#include "../../tools/diagnostics/contact_scaling.c"

static void test_small_pixel_pairs_do_not_scan_unused_tile_padding(void) {
    for(unsigned shape=0;shape<3;shape++) {
        measure(8,shape,2);
        ASSERT_TRUE(last_emitted>0u);
        ASSERT_TRUE(last_visited<=200u);
    }
    PASS();
}
int main(void) {
    RUN(test_small_pixel_pairs_do_not_scan_unused_tile_padding);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
