#define DC_CONTACT_SCALING_NO_MAIN
#include "../../tools/diagnostics/contact_scaling.c"

static void test_dense_negative_candidate_uses_bounded_presence_work(void) {
    measure(8, 0, 3);
    double sparse = last_contact_ms;
    ASSERT_EQ(last_emitted, 0u);
    measure(8, 0, 4);
    double dense = last_contact_ms;
    ASSERT_EQ(last_emitted, 0u);
    printf("negative terrain presence: sparse %.6fms dense %.6fms\n",
           sparse, dense);
    ASSERT_EQ(last_visited,0u);
    PASS();
}

int main(void) {
    RUN(test_dense_negative_candidate_uses_bounded_presence_work);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
