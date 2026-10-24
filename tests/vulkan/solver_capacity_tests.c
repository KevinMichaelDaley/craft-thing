#define main capacity_rigid_fixture_main
#include "rigid_solver_tests.c"
#undef main
#include "../../src/vulkan/runtime_body_internal.h"

static bool capacity_pipeline(dc_gpu_t *g,bool reference,char *err,uint32_t cap) {
    VkShaderModule module=VK_NULL_HANDLE;
    const char *path=reference?"build/diagnostics/solver_capacity_reference.spv":"build/diagnostics/solver_capacity_profile.spv";
    if(!dc_gpu_load_shader_module(g,path,&module,err,cap)) return false;
    uint32_t value=6;VkSpecializationMapEntry entry={0,0,4};
    VkSpecializationInfo spec={1,&entry,sizeof(value),&value};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkResult result=vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    vkDestroyShaderModule(g->device,module,NULL);
    if(result!=VK_SUCCESS) return false;
    vkDestroyPipeline(g->device,g->solver_pipelines[6],NULL);g->solver_pipelines[6]=pipeline;return true;
}
static void test_solver_reads_dynamic_layout_once_and_matches_uncached_after_growth(void) {
    char err[256]={0};dc_gpu_t *g[2]={grid(),grid()};ASSERT_TRUE(g[0]&&g[1]);
    VkBuffer buffer[2];VkDeviceMemory memory[2];uint32_t *counts[2];
    for(unsigned variant=0;variant<2;variant++) {
        ASSERT_TRUE(g[variant]->runtime_body_consumers);
        ASSERT_TRUE(dc_gpu_set_rigid_solver(g[variant],true));
        for(unsigned i=0;i<8;i++) {
            dc_gpu_body_shape_t shape=rectangle(i%2?DC_MATERIAL_WOOD:DC_MATERIAL_STONE,4,4);
            ASSERT_TRUE(dc_gpu_spawn_convex_body(g[variant],box(i+1,30+(i%4)*4,38+(i/4)*4,4,4),&shape,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_body_motion(g[variant],i+1,.01f*(float)i,0,0,err,sizeof(err)));
        }
        ASSERT_TRUE(capacity_pipeline(g[variant],variant==0,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(g[variant],16,&buffer[variant],&memory[variant],(void **)&counts[variant],err,sizeof(err)));
        memset(counts[variant],0,16);
        VkDescriptorBufferInfo binding={buffer[variant],0,16};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g[variant]->descriptor,
            .dstBinding=31,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&binding};
        vkUpdateDescriptorSets(g[variant]->device,1,&write,0,NULL);g[variant]->descriptor_revision++;
    }
    bool exact=true;
    for(unsigned step=0;step<4;step++) {
        for(unsigned variant=0;variant<2;variant++) {
            if(step==2) ASSERT_TRUE(dc_gpu_runtime_queues_reserve(g[variant],8193,16385,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_rigid_step(g[variant],err,sizeof(err)));
        }
        for(unsigned slot=0;slot<8;slot++)
            exact=exact&&memcmp(dc_gpu_body_record(g[0],slot),dc_gpu_body_record(g[1],slot),sizeof(dc_gpu_body_record_t))==0;
    }
    unsigned queries[2]={counts[0][0],counts[1][0]},invocations[2]={counts[0][1],counts[1][1]};
    unsigned tagged_contacts[2]={counts[0][2],counts[1][2]};
    printf("solver metadata reads%u/%u -> %u/%u invocations, exact bodies%d\n",
        queries[0],invocations[0],queries[1],invocations[1],exact);
    printf("generic contact metadata resolutions%u -> %u\n",tagged_contacts[0],tagged_contacts[1]);
    for(unsigned variant=0;variant<2;variant++) {
        vkUnmapMemory(g[variant]->device,memory[variant]);vkDestroyBuffer(g[variant]->device,buffer[variant],NULL);
        vkFreeMemory(g[variant]->device,memory[variant],NULL);dc_gpu_destroy(g[variant]);
    }
    ASSERT_TRUE(exact);ASSERT_TRUE(invocations[0]>0&&invocations[1]>0);
    ASSERT_EQ(invocations[0],invocations[1]);ASSERT_TRUE(queries[0]>2u*invocations[0]);
    ASSERT_TRUE(queries[1]<=2u*invocations[1]);
    ASSERT_TRUE(tagged_contacts[0]>0u);ASSERT_EQ(tagged_contacts[1],0u);PASS();
}
int main(void) {RUN(test_solver_reads_dynamic_layout_once_and_matches_uncached_after_growth);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
