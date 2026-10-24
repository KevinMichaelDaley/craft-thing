#define DC_PIXEL_BOUNDARY_MAIN contact_boundary_fixture_main
#include "pixel_boundary_tests.c"
#undef DC_PIXEL_BOUNDARY_MAIN

static void test_cooperative_boundaries_match_scalar_with_fewer_reads(void) {
    for(unsigned pattern=0;pattern<4;pattern++) {
        property_fixture_t f={0};boundary_fixture_pattern(&f,pattern);
        fixture_release_pipeline(&f.probe);uint32_t bindings[]={0,4};
        ASSERT_TRUE(fixture_body_pipeline(&f.probe,&f.gpu,f.body,
            "build/diagnostics/contact_boundary_probe.spv",bindings,2,UINT32_MAX));
        memset(f.probe.mapped,0,39u*sizeof(uint32_t));fixture_run(&f,"cooperative contact boundaries");
        unsigned errors=f.probe.mapped[0],scalar=f.probe.mapped[1],cooperative=f.probe.mapped[2],cases=f.probe.mapped[3];
        printf("pattern%u: %u boundary mismatches, reads %u -> %u, cases %u subgroup%u\n",
            pattern,errors,scalar,cooperative,cases,f.probe.mapped[4]);
        fixture_stop(&f);ASSERT_EQ(errors,0u);ASSERT_EQ(cases,1704u);
        ASSERT_TRUE(cooperative<scalar);if(pattern==0u) ASSERT_TRUE(cooperative*2u<scalar);
    }
    PASS();
}
int main(void) {
    RUN(test_cooperative_boundaries_match_scalar_with_fewer_reads);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
