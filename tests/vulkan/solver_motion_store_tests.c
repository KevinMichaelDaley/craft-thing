#define main motion_device_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static uint32_t float_bits(float value) {uint32_t word;memcpy(&word,&value,4);return word;}

static VkPipeline motion_pipeline(probe_t *p,dc_gpu_t *gpu,uint32_t phase,bool sparse) {
    VkShaderModule module=VK_NULL_HANDLE;char err[256];
    if(!dc_gpu_load_shader_module(gpu,"build/diagnostics/solver_motion_profile.comp.spv",&module,err,sizeof(err))) return VK_NULL_HANDLE;
    uint32_t values[]={phase,sparse};
    VkSpecializationMapEntry entries[]={{0,0,4},{1,4,4}};
    VkSpecializationInfo spec={2,entries,sizeof(values),values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main",.pSpecializationInfo=&spec},
        .layout=p->layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkResult status=vkCreateComputePipelines(p->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    vkDestroyShaderModule(p->device,module,NULL);
    return status==VK_SUCCESS?pipeline:VK_NULL_HANDLE;
}

static void test_motion_phases_preserve_every_body_word_with_narrow_writes(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical};
    enum {WIDTH=128,HEIGHT=128,RECORDS=64,CONTACT_BASE=8+32+4096*8,
          SOLVER_BASE=CONTACT_BASE+12+8192*32,BODY_STATE=SOLVER_BASE+16};
    VkDeviceSize bytes=RECORDS*sizeof(dc_gpu_body_record_t)+(SOLVER_BASE+256u)*4u;
    VkDeviceSize fixture_bytes=dc_gpu_body_storage_bytes(&gpu);
    if(bytes<fixture_bytes) bytes=fixture_bytes;
    VkBuffer storage,profile;VkDeviceMemory storage_memory,profile_memory;
    uint32_t *mapped,*counts;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,bytes,&storage,&storage_memory,(void **)&mapped,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,16,&profile,&profile_memory,(void **)&counts,err,sizeof(err)));
    uint8_t *initial=calloc(1,(size_t)bytes),*reference=malloc((size_t)bytes);
    ASSERT_TRUE(initial&&reference);
    dc_gpu_body_record_t *record=(void *)initial;
    for(unsigned word=0;word<sizeof(*record)/4u;word++)
        memcpy((uint8_t *)record+word*4u,&(uint32_t){0x3f000000u+word},4u);
    record->reserved[0]=0;record->motion_flags=0;record->shape.count=0;
    memset(record->chunk_x,0,sizeof(record->chunk_x));memset(record->chunk_y,0,sizeof(record->chunk_y));
    record->body=(dc_gpu_body_t){.x_fp=(64<<16)-128,.y_fp=25<<16,.vx_fp=12000,.vy_fp=-18000,
        .width=3,.height=7,.id=123,.active=1};
    record->local_x_fp=record->body.x_fp;record->local_y_fp=record->body.y_fp;
    record->visible=1;record->radius=4;record->angle=.13f;record->angular_velocity=.017f;
    record->center[0]=1.5f;record->center[1]=3.5f;
    record->inverse_mass=record->inverse_inertia=record->mass=record->inertia=1;
    record->reserved[1]=0x76543210u;record->rest_ticks=17;
    uint32_t *data=(uint32_t *)(initial+RECORDS*sizeof(*record));
    data[BODY_STATE+13]=1;data[BODY_STATE+16]=700;data[BODY_STATE+17]=(uint32_t)-1200;
    data[BODY_STATE+18]=100;data[BODY_STATE+6]=float_bits(63.9f);data[BODY_STATE+7]=float_bits(24.9f);
    data[BODY_STATE+8]=float_bits(.12f);
    p.buffer=storage;p.mapped=mapped;
    uint32_t bindings[]={1,2,3,4,10,19,20,21,23,24,27,31};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,storage,"build/diagnostics/solver_motion_profile.comp.spv",bindings,12,7));
    VkDescriptorBufferInfo buffers[12];
    VkWriteDescriptorSet writes[12];
    for(unsigned i=0;i<12;i++) {
        buffers[i]=(VkDescriptorBufferInfo){i==11?profile:storage,0,i==11?16:bytes};
        writes[i]=(VkWriteDescriptorSet){
        .sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,.dstBinding=bindings[i],
        .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=buffers+i};
    }
    vkUpdateDescriptorSets(p.device,12,writes,0,NULL);
    bool exact=true,narrow=true;
    const uint32_t phases[]={2,7,8,10};
    for(unsigned phase=0;phase<4;phase++) for(unsigned variant=0;variant<2;variant++) {
        memcpy(mapped,initial,(size_t)bytes);memset(counts,0,16);
        vkDestroyPipeline(p.device,p.pipeline,NULL);p.pipeline=motion_pipeline(&p,&gpu,phases[phase],variant!=0);
        ASSERT_TRUE(p.pipeline&&begin(&p));
        vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
        uint32_t push[8]={WIDTH,HEIGHT,phases[phase],1,0,0,0,0};
        vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(p.command,1,1,1);host_barrier(&p,1);
        ASSERT_TRUE(vkEndCommandBuffer(p.command)==VK_SUCCESS&&submit(&p,"solver motion publication",1));
        if(!variant) memcpy(reference,mapped,(size_t)bytes);
        else {exact=exact&&memcmp(reference,mapped,(size_t)bytes)==0;narrow=narrow&&counts[0]<=14u;}
        printf("phase%u variant%u: body words stored%u, exact%d\n",phases[phase],variant,counts[0],exact);
    }
    fixture_release_pipeline(&p);
    vkUnmapMemory(p.device,storage_memory);vkUnmapMemory(p.device,profile_memory);
    vkDestroyBuffer(p.device,storage,NULL);vkDestroyBuffer(p.device,profile,NULL);
    vkFreeMemory(p.device,storage_memory,NULL);vkFreeMemory(p.device,profile_memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);
    vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);free(initial);free(reference);
    ASSERT_TRUE(exact);ASSERT_TRUE(narrow);PASS();
}
int main(void) {
    RUN(test_motion_phases_preserve_every_body_word_with_narrow_writes);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
