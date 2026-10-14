#include <math.h>
#include <stdio.h>

#include "ferrum/renderer/texture_gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_gpu_matches_reference_on_mixed_owners(void) {
    dc_gpu_t *gpu;char err[256];fr_texture_gpu_t *textures;
    ASSERT_TRUE(dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(fr_texture_gpu_create(gpu,96,&textures,err,sizeof(err)));
    fr_appearance_owner_t owners[96];uint32_t styles[96];
    for(uint32_t i=0;i<96;i++) {
        owners[i]=(fr_appearance_owner_t){.identity=91,.generation=3,.x=(int)i-32,.y=11,
            .material=i<32?7u:1u,.visual_scale=1};styles[i]=i>=64?FR_TEXTURE_BRICK:0;
    }
    ASSERT_TRUE(fr_texture_gpu_upload(textures,owners,styles,96));
    ASSERT_TRUE(fr_texture_gpu_generate(textures,err,sizeof(err)));
    for(uint32_t i=0;i<96;i++) {
        fr_appearance_texel_t a,b=fr_texture_sample(owners[i],styles[i]);
        ASSERT_TRUE(fr_texture_gpu_read(textures,i,&a));
        for(uint32_t c=0;c<3;c++) {
            ASSERT_TRUE(fabsf(a.color[c]-b.color[c])<.00002f);
            ASSERT_TRUE(fabsf(a.normal[c]-b.normal[c])<.00002f);
        }
        ASSERT_TRUE(fabsf(a.relief-b.relief)<.00002f);
    }
    fr_texture_gpu_destroy(textures);dc_gpu_destroy(gpu);PASS();
}
int main(void) {RUN(test_gpu_matches_reference_on_mixed_owners);printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
