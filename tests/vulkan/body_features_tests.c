#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/body_features_gpu.h"
static int g_pass,g_fail;
static bool sync_supported,address_supported,subgroup_supported;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
VKAPI_ATTR void VKAPI_CALL __wrap_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physical,VkPhysicalDeviceFeatures2 *features) {
    (void)physical;void *next=features->pNext;
    while(next) {
        VkStructureType type;memcpy(&type,next,sizeof(type));void *following;
        memcpy(&following,(char *)next+offsetof(VkBaseOutStructure,pNext),sizeof(following));
        if(type==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)
            ((VkPhysicalDeviceSynchronization2Features *)next)->synchronization2=sync_supported;
        if(type==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES) {
            VkPhysicalDeviceBufferDeviceAddressFeatures *address=next;
            address->bufferDeviceAddress=address_supported;
            address->bufferDeviceAddressCaptureReplay=VK_TRUE;address->bufferDeviceAddressMultiDevice=VK_TRUE;
        }
        if(type==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES)
            ((VkPhysicalDeviceSubgroupSizeControlFeatures *)next)->subgroupSizeControl=subgroup_supported;
        next=following;
    }
}
static void test_success_owns_feature_chain_and_enables_only_required_features(void) {
    sync_supported=address_supported=true;dc_gpu_body_features_t out={0};char err[128];
    ASSERT_TRUE(dc_gpu_body_features_query((VkPhysicalDevice)(uintptr_t)1,&out,err,sizeof(err)));
    ASSERT_TRUE(out.synchronization.synchronization2&&out.addresses.bufferDeviceAddress);
    ASSERT_TRUE(out.synchronization.pNext==&out.addresses);ASSERT_TRUE(out.addresses.pNext!=NULL);
    ASSERT_TRUE(!out.addresses.bufferDeviceAddressCaptureReplay&&!out.addresses.bufferDeviceAddressMultiDevice);PASS();
}
static void test_missing_feature_preserves_destination_and_reports_exact_requirement(void) {
    for(uint32_t missing=0;missing<2;missing++) {
        sync_supported=missing!=0;address_supported=missing!=1;
        dc_gpu_body_features_t out,before;memset(&out,0x73,sizeof(out));before=out;char err[128]={0};
        ASSERT_TRUE(!dc_gpu_body_features_query((VkPhysicalDevice)(uintptr_t)1,&out,err,sizeof(err)));
        ASSERT_TRUE(memcmp(&out,&before,sizeof(out))==0);
        ASSERT_TRUE(strstr(err,missing==0?"synchronization2":"bufferDeviceAddress")!=NULL);
    }PASS();
}
static void test_optional_subgroup_control_is_enabled_only_when_available(void) {
    sync_supported=address_supported=true;
    for(unsigned supported=0;supported<2;supported++) {
        subgroup_supported=supported;dc_gpu_body_features_t out={0};char err[128];
        ASSERT_TRUE(dc_gpu_body_features_query((VkPhysicalDevice)(uintptr_t)1,&out,err,sizeof(err)));
        VkBaseOutStructure *node=out.addresses.pNext;
        ASSERT_TRUE(node&&node->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES);
        VkPhysicalDeviceSubgroupSizeControlFeatures *subgroup=(void *)node;
        ASSERT_EQ(subgroup->subgroupSizeControl,supported);
        ASSERT_TRUE(!subgroup->computeFullSubgroups&&subgroup->pNext==NULL);
    }
    PASS();
}
int main(void) {RUN(test_optional_subgroup_control_is_enabled_only_when_available);RUN(test_success_owns_feature_chain_and_enables_only_required_features);RUN(test_missing_feature_preserves_destination_and_reports_exact_requirement);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
