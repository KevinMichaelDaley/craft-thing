#define DC_PIXEL_PROPERTY_MAIN contact_quad_fixture_main
#include "pixel_properties_gpu_tests.c"
#undef DC_PIXEL_PROPERTY_MAIN

static void test_quad_contacts_match_frozen_ordered_sat_without_dynamic_loops(void) {
    property_fixture_t f={0};fixture_start(&f,1);
    fixture_release_pipeline(&f.probe);uint32_t bindings[]={0,4};
    ASSERT_TRUE(fixture_body_pipeline(&f.probe,&f.gpu,f.body,
        "build/diagnostics/contact_quad_probe.spv",bindings,2,UINT32_MAX));
    memset(f.probe.mapped,0,39u*sizeof(uint32_t));
    fixture_run(&f,"ordered quad contact geometry");
    uint32_t errors=f.probe.mapped[0],cases=f.probe.mapped[1],loops=f.probe.mapped[2];
    printf("quad contact cases %u exact errors %u dynamic iterations %u\n",cases,errors,loops);
    unsigned general_errors=f.probe.mapped[3],general_cases=f.probe.mapped[4];
    printf("mixed pentagon/quad cases %u exact errors %u\n",general_cases,general_errors);
    fixture_stop(&f);ASSERT_EQ(cases,32768u);ASSERT_EQ(errors,0u);ASSERT_EQ(loops,0u);
    ASSERT_EQ(general_cases,cases);ASSERT_EQ(general_errors,0u);PASS();
}
int main(void) {
    RUN(test_quad_contacts_match_frozen_ordered_sat_without_dynamic_loops);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
