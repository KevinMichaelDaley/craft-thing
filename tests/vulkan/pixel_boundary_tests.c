#define DC_PIXEL_PROPERTY_MAIN boundary_property_fixture_main
#include "pixel_properties_gpu_tests.c"
#undef DC_PIXEL_PROPERTY_MAIN

static void test_packed_boundary_matches_sparse_concave_reference_with_bounded_reads(void) {
    for(unsigned pattern=0;pattern<4;pattern++) {
        property_fixture_t f={0};fixture_start(&f,1);
        dc_gpu_body_record_t *body=f.gpu.body_mapped;
        body->body.width=32;body->body.height=32;
        body->angle=(pattern&1u)?0.371f:-0.217f;body->center[0]=5.25f;body->center[1]=14.125f;
        body->piece_count=DC_GPU_PIXEL_BODY_FLAG|768u;
        memset(f.shape,0,144u*sizeof(uint32_t));
        const uint32_t header[]={144,3,2,2,0,0,32,32,12,45,78,0};
        memcpy(f.shape,header,sizeof(header));
        for(unsigned tile=0;tile<3;tile++) {
            unsigned start=12+tile*33;
            f.shape[start]=tile==0?0:tile==1?1:65536;
            for(unsigned cell=0;cell<256;cell++) {
                unsigned x=(tile==1?16:0)+cell%16,y=(tile==2?16:0)+cell/16;
                bool occupied=pattern==0||
                    (pattern==1?((x*7+y*19)%11)!=0:
                     pattern==2?(x<5||y<5||x==19||y==19):
                     (x%8==0||y%8==0||x==y));
                if(occupied) f.shape[start+1+cell/8]|=(1u+(x+y)%5u)<<((cell%8)*4);
            }
        }
        fixture_release_pipeline(&f.probe);
        uint32_t bindings[]={0,4};
        ASSERT_TRUE(fixture_body_pipeline(&f.probe,&f.gpu,f.body,
            "build/diagnostics/pixel_boundary_probe.spv",bindings,2,UINT32_MAX));
        memset(f.probe.mapped,0,39u*sizeof(uint32_t));
        fixture_run(&f,"packed pixel boundaries");
        uint32_t errors=f.probe.mapped[0],interior=f.probe.mapped[1],tested=f.probe.mapped[2];
        printf("pattern%u: %u exact mismatches, %u interior reads, %u edge reads, %u candidates\n",
            pattern,errors,interior,f.probe.mapped[3],tested);
        uint32_t polygon_reads=f.probe.mapped[4],polygon_errors=f.probe.mapped[5];
        printf("polygon reads%u exact vertex mismatches%u\n",polygon_reads,polygon_errors);
        fixture_stop(&f);
        ASSERT_EQ(errors,0u);ASSERT_EQ(tested,1024u);ASSERT_TRUE(interior<=10u);ASSERT_EQ(polygon_errors,0u);ASSERT_TRUE(polygon_reads<=3u);
    }
    PASS();
}
int main(void) {
    RUN(test_packed_boundary_matches_sparse_concave_reference_with_bounded_reads);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
