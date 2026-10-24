#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static unsigned transport_count,wrong_options,pipeline_count;
static bool expected_long,expected_pages;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

bool __wrap_dc_gpu_load_shader_module(dc_gpu_t *g,const char *path,VkShaderModule *module,char *err,uint32_t cap) {
    (void)g;(void)err;(void)cap;
    *module=(VkShaderModule)(uintptr_t)(strstr(path,"fluid_transport")?1u:2u);return true;
}
bool __wrap_dc_gpu_pressure_work_init(dc_gpu_t *g,char *err,uint32_t cap) {
    (void)g;(void)err;(void)cap;return true;
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(VkPhysicalDevice d,VkPhysicalDeviceFeatures2 *f) {(void)d;(void)f;}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(VkPhysicalDevice d,VkPhysicalDeviceProperties2 *p) {
    (void)d;p->properties.limits.maxComputeSharedMemorySize=65536u;
    if(p->pNext&&((VkBaseOutStructure *)p->pNext)->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES)
        ((VkPhysicalDeviceSubgroupProperties *)p->pNext)->subgroupSize=16u;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice d,VkShaderModule m,const VkAllocationCallbacks *a) {(void)d;(void)m;(void)a;}
static uint32_t setting(const VkSpecializationInfo *s,uint32_t id) {
    for(unsigned i=0;s&&i<s->mapEntryCount;i++) if(s->pMapEntries[i].constantID==id) {
        uint32_t value=0;memcpy(&value,(const char *)s->pData+s->pMapEntries[i].offset,sizeof(value));return value;
    }
    return 0;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice d,VkPipelineCache c,uint32_t n,
    const VkComputePipelineCreateInfo *infos,const VkAllocationCallbacks *a,VkPipeline *pipelines) {
    (void)d;(void)c;(void)a;
    for(unsigned i=0;i<n;i++) {
        bool transport=infos[i].stage.module==(VkShaderModule)(uintptr_t)1u;
        transport_count+=transport;
        wrong_options+=setting(infos[i].stage.pSpecializationInfo,6u)!=(transport&&expected_long);
        wrong_options+=setting(infos[i].stage.pSpecializationInfo,7u)!=(transport&&expected_pages);
        pipelines[i]=(VkPipeline)(uintptr_t)++pipeline_count;
    }
    return VK_SUCCESS;
}
static void options_case(bool scan,bool pages) {
    expected_long=scan;expected_pages=pages;transport_count=wrong_options=0;
    if(scan) setenv("DC_TRANSPORT_LONG_SCAN","1",1);else unsetenv("DC_TRANSPORT_LONG_SCAN");
    if(pages) setenv("DC_TRANSPORT_PAGE_CACHE","1",1);else unsetenv("DC_TRANSPORT_PAGE_CACHE");
    dc_gpu_t g={.width=640,.height=448};char err[256];
    ASSERT_TRUE(dc_gpu_fluid_pipeline_init(&g,err,sizeof(err)));
    ASSERT_EQ(transport_count,4u);ASSERT_EQ(wrong_options,0u);PASS();
}
static void test_default_transport_keeps_experiments_off(void) {options_case(false,false);}
static void test_transport_options_do_not_change_other_fluid_pipelines(void) {options_case(true,true);}
static void test_transport_options_are_independent(void) {options_case(true,false);options_case(false,true);}
int main(void) {
    RUN(test_default_transport_keeps_experiments_off);
    RUN(test_transport_options_do_not_change_other_fluid_pipelines);
    RUN(test_transport_options_are_independent);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
