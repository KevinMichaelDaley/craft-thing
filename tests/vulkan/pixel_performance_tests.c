#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s\n", __LINE__, #e); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() ++g_pass

static void test_two_complex_pixel_bodies_meet_real_time_gpu_budget(void) {
    char err[256]={0}; dc_gpu_t *g=NULL;
    ASSERT_TRUE(dc_gpu_create(&g,480,270,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk)); ASSERT_TRUE(chunk);
    for(unsigned cy=0;cy<5;cy++) for(unsigned cx=0;cx<8;cx++) {
        *chunk=(dc_chunk_t){.coord={cx,cy}};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            unsigned wx=cx*64+x,wy=cy*64+y,material=DC_MATERIAL_AIR;
            if(wy==244 && wx<480) material=DC_MATERIAL_STONE;
            else if(wx>=180 && wx<260 && wy>=30 && wy<94 &&
                    (wx<186 || wx>=254 || wy>=88)) material=DC_MATERIAL_WOOD;
            else if(wx>=198 && wx<270 && wy>=115 && wy<163 &&
                    (wx<204 || (wy-115)%6<3)) material=wx<210?DC_MATERIAL_STONE:DC_MATERIAL_DENSE_WOOD;
            chunk->cells[y*64+x].material=material;
        }
        unsigned slot=cy*8+cx;
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,cx,cy,slot,err,sizeof(err)));
    }
    free(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(g,2));
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000001u,0,.005f,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,0x80000002u,0,-.003f,0,err,sizeof(err)));
    for(unsigned i=0;i<7;i++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    uint64_t rigid=0,total=0;
    for(unsigned i=0;i<24;i++) {
        dc_gpu_tick_capture_t capture;
        ASSERT_TRUE(dc_gpu_tick_capture(g,&capture,err,sizeof(err)));
        rigid+=capture.stages[0].gpu_ns;
        for(unsigned stage=0;stage<3;stage++) total+=capture.stages[stage].gpu_ns;
        dc_gpu_contact_stats_t stats;
        ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,NULL,0,err,sizeof(err)));
        ASSERT_EQ(stats.overflow,0u);
    }
    dc_gpu_body_t first,second;
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000001u,&first,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(g,0x80000002u,&second,err,sizeof(err)));
    ASSERT_TRUE(first.active && second.active);
    double rigid_ms=rigid/(24.0*1e6),total_ms=total/(24.0*1e6);
    printf("complex pixel mean GPU: rigid %.3f ms, whole tick %.3f ms\n",rigid_ms,total_ms);
    dc_gpu_destroy(g);
    ASSERT_TRUE(rigid_ms<14.0);
    ASSERT_TRUE(total_ms<1000.0/60.0);
    PASS();
}
int main(void) {
    RUN(test_two_complex_pixel_bodies_meet_real_time_gpu_budget);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail ? 1 : 0;
}
