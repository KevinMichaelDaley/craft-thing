#define DC_PROBE_WORDS 8192
#define main reactive_probe_fixture_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main

#include <math.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/fire/material_profile.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
enum { CASES=256,PROFILE_WORD=16,CASE_WORD=656,OUTPUT_WORD=3216 };

static void cleanup(probe_t *p) {
    vkDestroyPipeline(p->device,p->pipeline,NULL);vkDestroyShaderModule(p->device,p->shader,NULL);
    vkDestroyPipelineLayout(p->device,p->layout,NULL);vkDestroyDescriptorPool(p->device,p->descriptor_pool,NULL);
    vkDestroyDescriptorSetLayout(p->device,p->descriptor_layout,NULL);vkUnmapMemory(p->device,p->memory);
    vkDestroyBuffer(p->device,p->buffer,NULL);vkFreeMemory(p->device,p->memory,NULL);
    vkDestroyFence(p->device,p->fence,NULL);vkDestroyCommandPool(p->device,p->pool,NULL);
    vkDestroyDevice(p->device,NULL);vkDestroyInstance(p->instance,NULL);
}
static bool dispatch(probe_t *p) {
    if(!begin(p)) return false;
    VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p->command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,1,&memory,0,NULL,0,NULL);
    vkCmdBindPipeline(p->command,VK_PIPELINE_BIND_POINT_COMPUTE,p->pipeline);
    vkCmdBindDescriptorSets(p->command,VK_PIPELINE_BIND_POINT_COMPUTE,p->layout,0,1,&p->descriptor,0,NULL);
    vkCmdDispatch(p->command,(CASES+63u)/64u,1,1);host_barrier(p,1);
    return check(vkEndCommandBuffer(p->command),"reactive profile command end")&&submit(p,"reactive profiles",1);
}
static void test_gpu_profiles_match_cpu_rates_and_ignore_visual_pigment(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));ASSERT_TRUE(allocate_buffer(&p));
    ASSERT_TRUE(create_pipeline(&p,"build/diagnostics/reactive_profile_probe.spv"));
    memset(p.mapped,0,PROBE_WORDS*sizeof(uint32_t));
    p.mapped[0]=CASES;p.mapped[1]=DC_MATERIAL_COUNT;
    float oxygen[]={.12f,.21f};memcpy(p.mapped+2,oxygen,sizeof(oxygen));
    for(uint32_t id=0;id<DC_MATERIAL_COUNT;id++)
        memcpy(p.mapped+PROFILE_WORD+id*32u,fr_reactive_profile(id),sizeof(fr_reactive_profile_t));
    fr_reactive_ignition_rates_t expected[CASES];
    char err[128];
    for(uint32_t i=0;i<CASES;i++) {
        uint32_t material=i%DC_MATERIAL_COUNT,coat=i%3u?DC_MATERIAL_OIL:UINT32_MAX;
        fr_reactive_exposure_t e={.temperature_K=293.15f+(float)(i%11u)*73,
            .saturation=(float)(i%5u)/4,.coating_coverage=.5f,
            .substrate_oxidation=(float)(i%4u)/3,.coating_oxidation=.25f,
            .shock_energy_per_mass=i%7u==0?600:0,
            .oxygen_mass_fraction=i%4u==0?0:.21f,
            .bound_oxidizer_per_fuel=i%2u?.43f:0};
        uint32_t *record=p.mapped+CASE_WORD+i*10u;
        record[0]=material;record[1]=coat;memcpy(record+2,&e,sizeof(e));
        ASSERT_TRUE(fr_reactive_ignition_rates(fr_reactive_profile(material),
            coat==UINT32_MAX?NULL:fr_reactive_profile(coat),&e,&expected[i],err,sizeof(err)));
    }
    ASSERT_TRUE(dispatch(&p));
    uint32_t original[CASES*5];memcpy(original,p.mapped+OUTPUT_WORD,sizeof(original));
    unsigned layout_errors=0,rate_errors=0;
    for(uint32_t i=0;i<CASES;i++) {
        const fr_reactive_profile_t *profile=fr_reactive_profile(i%DC_MATERIAL_COUNT);
        uint32_t *out=p.mapped+OUTPUT_WORD+i*5u;
        layout_errors+=memcmp(out,&profile->density,4)!=0;
        layout_errors+=memcmp(out+1,&profile->primary_heat,4)!=0;
        layout_errors+=out[2]!=profile->flags;
        float rates[2];memcpy(rates,out+3,sizeof(rates));
        rate_errors+=fabsf(rates[0]-expected[i].substrate)>2e-6f*(1+expected[i].substrate);
        rate_errors+=fabsf(rates[1]-expected[i].coating)>2e-6f*(1+expected[i].coating);
    }
    p.mapped[6]=0x40a00000u;p.mapped[7]=0x3f800000u;p.mapped[8]=0x41400000u;
    ASSERT_TRUE(dispatch(&p));
    bool independent=memcmp(original,p.mapped+OUTPUT_WORD,sizeof(original))==0;
    printf("reactive GPU cases%u layout errors%u rate errors%u color-independent%d\n",
        CASES,layout_errors,rate_errors,independent);
    cleanup(&p);ASSERT_EQ(layout_errors,0u);ASSERT_EQ(rate_errors,0u);ASSERT_TRUE(independent);PASS();
}
int main(void) {
    RUN(test_gpu_profiles_match_cpu_rates_and_ignore_visual_pigment);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
