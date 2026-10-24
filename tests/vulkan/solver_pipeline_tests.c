#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
static unsigned pipelines,graph_pipelines,modules;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice d,const VkShaderModuleCreateInfo *i,
    const VkAllocationCallbacks *a,VkShaderModule *out) {
    (void)d;(void)i;(void)a;*out=(VkShaderModule)(uintptr_t)++modules;return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice d,VkShaderModule m,const VkAllocationCallbacks *a) {
    (void)d;(void)m;(void)a;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice d,VkPipelineCache c,uint32_t n,
    const VkComputePipelineCreateInfo *info,const VkAllocationCallbacks *a,VkPipeline *out) {
    (void)d;(void)c;(void)a;
    for(unsigned i=0;i<n;i++) {
        uint32_t phase=UINT32_MAX,capture=1u;
        const VkSpecializationInfo *spec=info[i].stage.pSpecializationInfo;
        for(unsigned entry=0;spec&&entry<spec->mapEntryCount;entry++) {
            const VkSpecializationMapEntry *map=spec->pMapEntries+entry;
            if(map->constantID==0u) memcpy(&phase,(const char *)spec->pData+map->offset,4);
            if(map->constantID==7u) memcpy(&capture,(const char *)spec->pData+map->offset,4);
        }
        graph_pipelines+=(phase>=12u&&phase<=14u)||((phase==6u||phase==9u)&&capture);
        out[i]=(VkPipeline)(uintptr_t)(100u+(++pipelines));
    }
    return VK_SUCCESS;
}
static void test_automatic_body_context_does_not_compile_graph_only_variants(void) {
    dc_gpu_t g={.device=(VkDevice)(uintptr_t)1,.pipeline_layout=(VkPipelineLayout)(uintptr_t)2};
    char err[128]={0};pipelines=graph_pipelines=modules=0u;
    ASSERT_TRUE(dc_gpu_solver_pipeline_init(&g,err,sizeof(err)));
    printf("solver constructor pipelines %u, unused graph variants %u\n",pipelines,graph_pipelines);
    ASSERT_EQ(graph_pipelines,0u);
    ASSERT_TRUE(g.solver_unstructured[0]&&g.solver_unstructured[1]);
    PASS();
}
int main(void) {
    RUN(test_automatic_body_context_does_not_compile_graph_only_variants);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
