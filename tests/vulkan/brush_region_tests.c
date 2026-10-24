#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static VkPipeline bound,paint_pipeline;
static uint32_t paint_groups;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

VKAPI_ATTR void VKAPI_CALL __real_vkCmdBindPipeline(VkCommandBuffer,VkPipelineBindPoint,VkPipeline);
VKAPI_ATTR void VKAPI_CALL __real_vkCmdDispatch(VkCommandBuffer,uint32_t,uint32_t,uint32_t);
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint p,VkPipeline pipeline) {
    bound=pipeline;__real_vkCmdBindPipeline(c,p,pipeline);
}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {
    if(bound==paint_pipeline) paint_groups+=x*y*z;
    __real_vkCmdDispatch(c,x,y,z);
}
static void test_brush_preserves_unrelated_pixels_and_bounds_gpu_work(void) {
    dc_gpu_t *gpu=NULL;char err[256]={0};
    ASSERT_TRUE(dc_gpu_create(&gpu,128,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(uint32_t slot=0;slot<2;slot++) {
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu,slot,0,slot,err,sizeof(err)));
    }
    uint32_t *before=malloc(8192u*4u),*after=malloc(8192u*4u);
    ASSERT_TRUE(before&&after);
    const uint32_t centers[3][2]={{63,31},{0,0},{127,63}};
    for(unsigned brush=0;brush<3;brush++) {
        ASSERT_TRUE(dc_gpu_pattern(gpu,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_readback(gpu,before,8192,err,sizeof(err)));
        paint_pipeline=gpu->pipeline;paint_groups=0;
        uint32_t cx=centers[brush][0],cy=centers[brush][1];
        ASSERT_TRUE(dc_gpu_paint_material(gpu,cx,cy,3,DC_MATERIAL_STONE,err,sizeof(err)));
        uint32_t groups=paint_groups;paint_pipeline=VK_NULL_HANDLE;
        ASSERT_TRUE(dc_gpu_readback(gpu,after,8192,err,sizeof(err)));
        unsigned unrelated=0;
        for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<128;x++) {
            int dx=(int)x-(int)cx,dy=(int)y-(int)cy;
            if(dx*dx+dy*dy>9) unrelated+=before[y*128+x]!=after[y*128+x];
            else ASSERT_EQ(after[y*128+x],0xff707070u);
        }
        printf("brush%u workgroups%u unrelated display changes%u\n",brush,groups,unrelated);
        ASSERT_EQ(unrelated,0u);ASSERT_EQ(groups,1u);
        for(uint32_t slot=0;slot<2;slot++) {
            ASSERT_TRUE(dc_gpu_download_chunk(gpu,slot,chunk,err,sizeof(err)));
            for(uint32_t y=0;y<64;y++) for(uint32_t x=0;x<64;x++) {
                int dx=(int)(slot*64+x)-(int)cx,dy=(int)y-(int)cy;
                if(dx*dx+dy*dy<=9) ASSERT_EQ(chunk->cells[y*64+x].material,DC_MATERIAL_STONE);
            }
        }
    }
    free(before);free(after);free(chunk);dc_gpu_destroy(gpu);PASS();
}
int main(void) {
    RUN(test_brush_preserves_unrelated_pixels_and_bounds_gpu_work);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
