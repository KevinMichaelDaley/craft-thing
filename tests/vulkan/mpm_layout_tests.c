#include <stdint.h>
#include <stdio.h>

#include "../../src/vulkan/mpm_activity_layout.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_scan_scratch_regions_are_disjoint_and_bounded(void) {
    const uint32_t dimensions[][2]={{1,1},{64,64},{640,360},{1280,720},{2113,17},{4097,4097},{UINT32_MAX,1}};
    for(unsigned i=0;i<sizeof(dimensions)/sizeof(*dimensions);i++) {
        dc_gpu_mpm_scan_layout_t l;ASSERT_TRUE(dc_gpu_mpm_scan_layout(dimensions[i][0],dimensions[i][1],&l));
        ASSERT_TRUE(l.levels>0&&l.levels<=DC_MPM_MAX_SCAN_LEVELS);
        ASSERT_EQ(l.groups,(l.tiles+255u)/256u);
        ASSERT_EQ(l.copy_word,7u+2u*l.tiles);ASSERT_EQ(l.count_word,l.copy_word+3u);
        ASSERT_EQ(l.small_word,l.count_word+1u);
        uint32_t offset=l.small_word+3u,count=l.groups;
        for(uint32_t level=0;level<l.levels;level++) {
            ASSERT_EQ(l.level_words[level],offset);ASSERT_EQ(l.level_counts[level],count);
            offset+=count;count=(count+255u)/256u;
        }
        ASSERT_EQ(count,1u);ASSERT_EQ(l.root_word,offset);
        ASSERT_EQ(l.words,offset+1u<DC_MPM_ACTIVITY_MIN_WORDS?DC_MPM_ACTIVITY_MIN_WORDS:offset+1u);
    }
    PASS();
}
static void test_large_domain_uses_multiple_prefix_levels(void) {
    dc_gpu_mpm_scan_layout_t l;ASSERT_TRUE(dc_gpu_mpm_scan_layout(4097,4097,&l));
    ASSERT_TRUE(l.tiles>65535u);ASSERT_EQ(l.levels,2u);
    ASSERT_EQ(l.level_counts[0],259u);ASSERT_EQ(l.level_counts[1],2u);PASS();
}
static void test_invalid_or_overflowing_dimensions_are_rejected(void) {
    dc_gpu_mpm_scan_layout_t l;
    ASSERT_TRUE(!dc_gpu_mpm_scan_layout(0,1,&l));ASSERT_TRUE(!dc_gpu_mpm_scan_layout(1,0,&l));
    ASSERT_TRUE(!dc_gpu_mpm_scan_layout(UINT32_MAX,UINT32_MAX,&l));
    ASSERT_TRUE(!dc_gpu_mpm_scan_layout(1,1,NULL));PASS();
}
int main(void) {
    RUN(test_scan_scratch_regions_are_disjoint_and_bounded);
    RUN(test_large_domain_uses_multiple_prefix_levels);
    RUN(test_invalid_or_overflowing_dimensions_are_rejected);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
