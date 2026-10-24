#include <stdio.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static VkDeviceSize activity_range;
static VkBuffer activity_buffer;
static VkDeviceSize component_range[3];
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

void __real_vkUpdateDescriptorSets(VkDevice,uint32_t,const VkWriteDescriptorSet *,uint32_t,const VkCopyDescriptorSet *);
void __wrap_vkUpdateDescriptorSets(VkDevice device,uint32_t count,const VkWriteDescriptorSet *writes,
                                  uint32_t copies,const VkCopyDescriptorSet *copy) {
    for(uint32_t i=0;i<count;i++) if(writes[i].dstBinding==27u&&writes[i].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        activity_range=writes[i].pBufferInfo[0].range;activity_buffer=writes[i].pBufferInfo[0].buffer;
    }
    for(uint32_t i=0;i<count;i++) if(writes[i].dstBinding>=28u&&writes[i].dstBinding<=30u&&
        writes[i].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        component_range[writes[i].dstBinding-28u]=writes[i].pBufferInfo[0].range;
    __real_vkUpdateDescriptorSets(device,count,writes,copies,copy);
}
static uint32_t required_words(uint32_t width,uint32_t height) {
    uint32_t tiles=((width+15u)/16u)*((height+15u)/16u),words=11u+2u*tiles;
    for(uint32_t count=(tiles+255u)/256u;;count=(count+255u)/256u) {
        words+=count;if(count<=256u) {words++;break;}
    }
    return words<DC_MPM_ACTIVITY_MIN_WORDS?DC_MPM_ACTIVITY_MIN_WORDS:words;
}
static void descriptor_case(uint32_t width,uint32_t height) {
    activity_range=0;activity_buffer=VK_NULL_HANDLE;dc_gpu_t *gpu=NULL;char err[256];
    ASSERT_TRUE(dc_gpu_create(&gpu,width,height,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    bool correct=activity_range==(VkDeviceSize)required_words(width,height)*4u&&activity_buffer==gpu->mpm_activity_buffer;
    VkDeviceSize visible=(VkDeviceSize)width*height,resident=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS;
    VkDeviceSize required=(2u*(visible>resident?visible:resident)+DC_CHUNK_CELLS)*sizeof(uint32_t);
    for(unsigned i=0;i<3;i++) correct=correct&&component_range[i]>=required;
    printf("%ux%u activity descriptor %llu bytes expected%u\n",width,height,
        (unsigned long long)activity_range,required_words(width,height)*4u);
    printf("terrain descriptor %llu bytes, required%llu independently of geometry arena\n",
        (unsigned long long)component_range[0],(unsigned long long)required);
    dc_gpu_destroy(gpu);ASSERT_TRUE(correct);PASS();
}
static void test_small_constructor_exposes_all_scan_scratch(void) {descriptor_case(64,64);}
static void test_native_constructor_exposes_all_scan_scratch(void) {descriptor_case(640,360);}
int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    RUN(test_small_constructor_exposes_all_scan_scratch);
    RUN(test_native_constructor_exposes_all_scan_scratch);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
