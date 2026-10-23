#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/body_storage_gpu.h"
static int g_pass,g_fail;
static uint32_t allocations,releases,fail_at,copies,updates,fills,barriers,host_buffers,initializers;
static uint64_t handle;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
bool __wrap_dc_gpu_address_buffer_create(dc_gpu_t *gpu,VkDeviceSize bytes,bool mapped,
    dc_gpu_address_buffer_t *out,char *err,uint32_t cap) {
    (void)gpu;(void)err;(void)cap;allocations++;host_buffers+=mapped;
    if(allocations==fail_at) return false;
    *out=(dc_gpu_address_buffer_t){.buffer=(VkBuffer)(uintptr_t)++handle,
        .address=handle*0x100000u,.bytes=bytes,.mapped=mapped?calloc(1,(size_t)bytes):NULL};return true;
}
void __wrap_dc_gpu_address_buffer_destroy(dc_gpu_t *gpu,dc_gpu_address_buffer_t *buffer) {
    (void)gpu;if(buffer->buffer){releases++;free(buffer->mapped);}*buffer=(dc_gpu_address_buffer_t){0};
}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdCopyBuffer(VkCommandBuffer c,VkBuffer a,VkBuffer b,uint32_t n,const VkBufferCopy *r) {(void)c;(void)a;(void)b;(void)n;(void)r;copies++;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdFillBuffer(VkCommandBuffer c,VkBuffer b,VkDeviceSize o,VkDeviceSize s,uint32_t d) {(void)c;(void)b;(void)o;(void)s;(void)d;fills++;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdUpdateBuffer(VkCommandBuffer c,VkBuffer b,VkDeviceSize o,VkDeviceSize s,const void *d) {(void)c;(void)b;(void)o;(void)s;(void)d;updates++;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdPipelineBarrier2(VkCommandBuffer c,const VkDependencyInfo *d) {(void)c;(void)d;barriers++;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdBindPipeline(VkCommandBuffer c,VkPipelineBindPoint b,VkPipeline p) {(void)c;(void)b;(void)p;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdPushConstants(VkCommandBuffer c,VkPipelineLayout l,VkShaderStageFlags s,uint32_t o,uint32_t n,const void *p) {(void)c;(void)l;(void)s;(void)o;(void)n;(void)p;}
VKAPI_ATTR void VKAPI_CALL __wrap_vkCmdDispatch(VkCommandBuffer c,uint32_t x,uint32_t y,uint32_t z) {(void)c;(void)x;(void)y;(void)z;initializers++;}
static void reset(void) {allocations=releases=fail_at=copies=updates=fills=barriers=host_buffers=initializers=0;}
static void test_directory_metadata_is_initialized_without_a_serial_host_page_upload(void) {
    reset();dc_gpu_t gpu={0};dc_gpu_body_store_t store={0};dc_gpu_body_growth_t growth={0};char err[128];
    ASSERT_TRUE(dc_gpu_body_growth_prepare(&gpu,&store,1025,&growth,err,sizeof(err)));
    uint32_t uploads=host_buffers;
    ASSERT_TRUE(dc_gpu_body_growth_discard(&gpu,&growth));
    ASSERT_EQ(uploads,0u);ASSERT_EQ(allocations,releases);PASS();
}
static void test_growth_preserves_store_until_submission_completion(void) {
    reset();dc_gpu_t gpu={0};dc_gpu_body_store_t store={0};dc_gpu_body_growth_t growth={0};char err[128];
    const uint32_t counts[]={65,200,1025};
    for(uint32_t i=0;i<3;i++) {
        uint32_t previous=store.slab_count;
        ASSERT_TRUE(dc_gpu_body_growth_prepare(&gpu,&store,counts[i],&growth,err,sizeof(err)));
        ASSERT_EQ(store.slab_count,previous);ASSERT_TRUE(growth.slab_count*64u>=counts[i]);
        ASSERT_TRUE(!dc_gpu_body_growth_finish(&gpu,&store,&growth,i+1));
        ASSERT_TRUE(dc_gpu_body_growth_record((VkCommandBuffer)(uintptr_t)1,(VkBuffer)(uintptr_t)2,
            (VkPipelineLayout)(uintptr_t)3,(VkPipeline)(uintptr_t)4,&store,&growth,i+1));
        ASSERT_TRUE(!dc_gpu_body_growth_discard(&gpu,&growth));
        ASSERT_TRUE(!dc_gpu_body_growth_finish(&gpu,&store,&growth,i));
        ASSERT_TRUE(dc_gpu_body_growth_finish(&gpu,&store,&growth,i+1));
        ASSERT_TRUE(store.slab_count*64u>=counts[i]);ASSERT_EQ(growth.submission,0u);
    }
    ASSERT_EQ(copies,2u);ASSERT_EQ(initializers,3u);ASSERT_EQ(fills,3u);ASSERT_TRUE(barriers>=6u);
    dc_gpu_body_store_destroy(&gpu,&store);ASSERT_EQ(allocations,releases);PASS();
}
static void test_partial_allocation_failure_preserves_active_store_and_destination(void) {
    for(uint32_t failure=1;failure<=2;failure++) {
        reset();fail_at=failure;dc_gpu_t gpu={0};dc_gpu_body_store_t store={.slab_count=2};
        dc_gpu_body_growth_t growth={0},before=growth;char err[128];
        ASSERT_TRUE(!dc_gpu_body_growth_prepare(&gpu,&store,201,&growth,err,sizeof(err)));
        ASSERT_EQ(store.slab_count,2u);ASSERT_TRUE(memcmp(&growth,&before,sizeof(growth))==0);
        ASSERT_EQ(releases,failure-1);ASSERT_EQ(copies,0u);
    }PASS();
}
int main(void) {RUN(test_directory_metadata_is_initialized_without_a_serial_host_page_upload);RUN(test_growth_preserves_store_until_submission_completion);RUN(test_partial_allocation_failure_preserves_active_store_and_destination);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
