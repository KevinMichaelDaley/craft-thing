#include <stdint.h>
#include <stdio.h>

#include "../../src/vulkan/fluid_transport_limits.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_transport_cache_respects_shared_memory_budget(void) {
    const uint32_t budgets[]={32768,49152,65536};
    const uint32_t lengths[]={1,360,640,1280,2048,2113,16384};
    for(unsigned b=0;b<3;b++) for(unsigned l=0;l<7;l++) {
        uint32_t cells=dc_gpu_transport_cache_cells(lengths[l],budgets[b]);
        ASSERT_TRUE(cells>0&&cells<=lengths[l]&&cells<=2048);
        ASSERT_TRUE(20u*cells+4u*((cells+31u)/32u)<=budgets[b]);
    }
    PASS();
}
static void test_native_360p_cache_is_not_reduced(void) {
    ASSERT_EQ(dc_gpu_transport_cache_cells(360,32768),360u);
    ASSERT_EQ(dc_gpu_transport_cache_cells(640,32768),640u);
    ASSERT_EQ(dc_gpu_transport_cache_cells(2113,65536),2048u);
    PASS();
}
int main(void) {
    RUN(test_transport_cache_respects_shared_memory_budget);
    RUN(test_native_360p_cache_is_not_reduced);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
