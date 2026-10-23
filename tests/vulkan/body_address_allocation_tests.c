#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/body_address_gpu.h"
static int g_pass,g_fail;
static unsigned created,destroyed,freed;
static bool fail_allocate,fail_bind;
static VkBufferUsageFlags usage;
static VkMemoryAllocateFlags allocation_flags;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
VkResult __wrap_vkCreateBuffer(VkDevice d,const VkBufferCreateInfo *i,const VkAllocationCallbacks *a,VkBuffer *out) {
    (void)d;(void)a;created++;usage=i->usage;*out=(VkBuffer)(uintptr_t)1;return VK_SUCCESS;
}
void __wrap_vkGetBufferMemoryRequirements(VkDevice d,VkBuffer b,VkMemoryRequirements *r) {(void)d;(void)b;*r=(VkMemoryRequirements){.size=4096,.alignment=16,.memoryTypeBits=1};}
void __wrap_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice d,VkPhysicalDeviceMemoryProperties *p) {
    (void)d;memset(p,0,sizeof(*p));p->memoryTypeCount=1;p->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}
VkResult __wrap_vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo *i,const VkAllocationCallbacks *a,VkDeviceMemory *out) {
    (void)d;(void)a;allocation_flags=i->pNext?((const VkMemoryAllocateFlagsInfo *)i->pNext)->flags:0;
    if(fail_allocate) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *out=(VkDeviceMemory)(uintptr_t)2;return VK_SUCCESS;
}
VkResult __wrap_vkBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory m,VkDeviceSize o) {(void)d;(void)b;(void)m;(void)o;return fail_bind?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;}
VkDeviceAddress __wrap_vkGetBufferDeviceAddress(VkDevice d,const VkBufferDeviceAddressInfo *i) {(void)d;(void)i;return 0x100000000ull;}
void __wrap_vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks *a) {(void)d;(void)b;(void)a;destroyed++;}
void __wrap_vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a) {(void)d;(void)m;(void)a;freed++;}
static void reset(void) {created=destroyed=freed=0;usage=0;allocation_flags=0;fail_allocate=fail_bind=false;}
static void test_resource_enables_real_address_usage_and_releases_allocation(void) {
    reset();dc_gpu_t gpu={0};dc_gpu_address_buffer_t out={0};char err[128];
    ASSERT_TRUE(dc_gpu_address_buffer_create(&gpu,2048,false,&out,err,sizeof(err)));
    ASSERT_TRUE(usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    ASSERT_TRUE(allocation_flags&VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
    ASSERT_EQ(out.address,0x100000000ull);ASSERT_EQ(out.bytes,2048u);
    dc_gpu_address_buffer_destroy(&gpu,&out);
    ASSERT_EQ(destroyed,1u);ASSERT_EQ(freed,1u);ASSERT_EQ(gpu.device_only_bytes,0u);PASS();
}
static void test_memory_failure_leaves_previous_destination_untouched(void) {
    reset();fail_allocate=true;dc_gpu_t gpu={0};char err[128];
    dc_gpu_address_buffer_t out={.bytes=777},before=out;
    ASSERT_TRUE(!dc_gpu_address_buffer_create(&gpu,2048,false,&out,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&out,&before,sizeof(out))==0);ASSERT_EQ(created,1u);
    ASSERT_EQ(destroyed,1u);ASSERT_EQ(freed,0u);PASS();
}
static void test_bind_failure_releases_memory_without_publishing(void) {
    reset();fail_bind=true;dc_gpu_t gpu={0};dc_gpu_address_buffer_t out={0};char err[128];
    ASSERT_TRUE(!dc_gpu_address_buffer_create(&gpu,2048,false,&out,err,sizeof(err)));
    ASSERT_EQ(out.buffer,VK_NULL_HANDLE);ASSERT_EQ(destroyed,1u);ASSERT_EQ(freed,1u);PASS();
}
int main(void) {
    RUN(test_resource_enables_real_address_usage_and_releases_allocation);
    RUN(test_memory_failure_leaves_previous_destination_untouched);
    RUN(test_bind_failure_releases_memory_without_publishing);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
