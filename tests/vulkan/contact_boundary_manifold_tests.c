#define _POSIX_C_SOURCE 200809L
#define DC_CONTACT_SCALING_NO_MAIN
#include "../../tools/diagnostics/contact_scaling.c"

static void test_rotated_concave_contacts_match_scalar_reference(void) {
    contact_test_angle=.37f;
    for(unsigned shape=0u;shape<=2u;shape++) {
        ASSERT_EQ(setenv("DC_CONTACT_SHADER","scalar-reference",1),0);
        measure(64,shape,2);
        uint64_t reference=last_contact_digest;unsigned count=last_emitted;
        ASSERT_EQ(unsetenv("DC_CONTACT_SHADER"),0);
        measure(64,shape,2);
        ASSERT_TRUE(count>0u);ASSERT_EQ(last_emitted,count);ASSERT_EQ(last_contact_digest,reference);
    }
    contact_test_angle=0.f;PASS();
}
int main(void) {
    RUN(test_rotated_concave_contacts_match_scalar_reference);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
