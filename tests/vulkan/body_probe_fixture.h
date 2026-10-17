#ifndef DC_BODY_PROBE_FIXTURE_H
#define DC_BODY_PROBE_FIXTURE_H
#define DC_PROBE_BUFFER_DEVICE_ADDRESS
#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

#include "../../src/vulkan/body_address_gpu.h"
#include "dungeoncraft/body_storage_layout.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static inline dc_body_address_t address_words(VkDeviceAddress address) {return (dc_body_address_t){(uint32_t)address,(uint32_t)(address>>32)};}
static inline VkPipeline create_body_probe_pipeline(probe_t *probe,dc_gpu_t *gpu,const char *path) {
    VkShaderModule module=VK_NULL_HANDLE;char err[128];
    if(!dc_gpu_load_shader_module(gpu,path,&module,err,sizeof(err))) {puts(err);return VK_NULL_HANDLE;}
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=probe->layout,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"}};
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkResult result=vkCreateComputePipelines(gpu->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    vkDestroyShaderModule(gpu->device,module,NULL);
    return result==VK_SUCCESS?pipeline:VK_NULL_HANDLE;
}
static void destroy_probe(probe_t *p) {
    vkDestroyPipeline(p->device,p->pipeline,NULL);
    vkDestroyShaderModule(p->device,p->shader,NULL);
    vkDestroyPipelineLayout(p->device,p->layout,NULL);
    vkDestroyDescriptorPool(p->device,p->descriptor_pool,NULL);
    vkDestroyDescriptorSetLayout(p->device,p->descriptor_layout,NULL);
    vkUnmapMemory(p->device,p->memory);
    vkDestroyBuffer(p->device,p->buffer,NULL);vkFreeMemory(p->device,p->memory,NULL);
    vkDestroyCommandPool(p->device,p->pool,NULL);vkDestroyFence(p->device,p->fence,NULL);
    vkDestroyDevice(p->device,NULL);vkDestroyInstance(p->instance,NULL);
}
#endif
