#define DC_SOLVER_MOTION_TEST_MAIN grain_fusion_motion_fixture_main
#include "solver_motion_store_tests.c"
#undef DC_SOLVER_MOTION_TEST_MAIN

static VkPipeline fusion_pipeline(probe_t *p,dc_gpu_t *gpu,uint32_t phase,bool fused) {
    VkShaderModule module=VK_NULL_HANDLE;char err[256];
    const char *path=phase==22u?"build/diagnostics/solver_batch_profile.comp.spv":
        "build/diagnostics/solver_motion_profile.comp.spv";
    if(!dc_gpu_load_shader_module(gpu,path,&module,err,sizeof(err))) return VK_NULL_HANDLE;
    uint32_t values[]={phase,fused};VkSpecializationMapEntry entries[]={{0,0,4},{6,4,4}};
    VkSpecializationInfo spec={2,entries,sizeof(values),values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=p->layout};
    VkPipeline pipeline=VK_NULL_HANDLE;vkCreateComputePipelines(p->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline);
    vkDestroyShaderModule(p->device,module,NULL);return pipeline;
}
static void run_grain_case(bool jacobi) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical};
    enum { CONTACT_BASE=8+32+4096*8,SOLVER_BASE=CONTACT_BASE+12+8192*32,
           BODY_STATE=SOLVER_BASE+16,N=6 };
    VkDeviceSize sizes[N]={dc_gpu_body_storage_bytes(&gpu),4096u*56u,4096u*16u,4096u*8u,16u,16u};
    VkBuffer buffers[N];VkDeviceMemory memory[N];void *mapped[N];uint8_t *initial[N],*expected[N];
    for(unsigned i=0;i<N;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,sizes[i],buffers+i,memory+i,mapped+i,err,sizeof(err)));
        initial[i]=calloc(1,sizes[i]);expected[i]=malloc(sizes[i]);ASSERT_TRUE(initial[i]&&expected[i]);
    }
    dc_gpu_body_record_t *body=(void *)initial[0];
    body->body=(dc_gpu_body_t){.id=123,.active=1,.x_fp=20<<16,.y_fp=20<<16,.width=3,.height=7};
    body->visible=1;body->inverse_mass=1;body->inverse_inertia=.125f;body->radius=4;
    uint32_t *data=(void *)(initial[0]+64*sizeof(*body));
    data[BODY_STATE+13]=1;data[CONTACT_BASE]=4;
    data[SOLVER_BASE+13]=1;
    data[BODY_STATE+16]=700;data[BODY_STATE+17]=(uint32_t)-1200;data[BODY_STATE+18]=100;
    ((uint32_t *)initial[4])[0]=1;
    for(unsigned i=0;i<4;i++) {
        uint32_t c=CONTACT_BASE+12+i*32,cell=20*64+20+i/2;
        data[c]=123;data[c+2]=3;data[c+7]=cell+1;
        data[c+20]=20<<16;data[c+21]=20<<16;
        data[c+26]=20<<16;data[c+27]=20<<16;
        if(jacobi) {
            data[c+8]=float_bits(1.f);data[c+11]=float_bits(.5f);
            data[c+20]=19*65536+32768;
        }
        int32_t *particle=(void *)(initial[1]+cell*56);
        particle[0]=(20+i/2)*65536+1234;particle[1]=20*65536-987;
        particle[2]=i<2?180000:-170000;particle[3]=-9000;
        particle[4]=particle[7]=(int32_t)float_bits(1);particle[10]=65536;particle[12]=2;
        uint32_t *scratch=(void *)(initial[2]+cell*16);
        scratch[0]=i<2?17000u:(uint32_t)-25000;scratch[1]=(uint32_t)-5000;
    }
    p.buffer=buffers[0];p.mapped=mapped[0];
    uint32_t bindings[]={1,2,3,4,10,19,20,21,23,24,25,27,31};
    unsigned indices[]={3,4,0,0,0,1,0,2,0,0,0,0,5};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,buffers[0],"build/diagnostics/solver_motion_profile.comp.spv",bindings,13,7));
    for(unsigned i=0;i<13;i++) {
        unsigned j=indices[i];VkDescriptorBufferInfo buffer={buffers[j],0,sizes[j]};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
        vkUpdateDescriptorSets(p.device,1,&write,0,NULL);
    }
    bool exact=true,changed=false;
    if(jacobi) for(unsigned batched=0;batched<2;batched++) {
        for(unsigned i=0;i<N;i++) memcpy(mapped[i],initial[i],sizes[i]);
        const uint32_t phases[]={15,4,5,6,7,16,22};VkPipeline pipelines[7]={0};
        for(unsigned i=0;i<(batched?7u:6u);i++) {
            pipelines[i]=fusion_pipeline(&p,&gpu,phases[i],false);ASSERT_TRUE(pipelines[i]);
        }
        ASSERT_TRUE(begin(&p));
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
        for(unsigned pass=0;pass<(batched?4u:27u);pass++) {
            unsigned stage=pass<3u?pass:batched?6u:3u+(pass-3u)%3u;
            unsigned epoch=pass<3u||batched?0u:1u+(pass-3u)/3u;
            uint32_t push[8]={128,128,phases[stage]|(epoch<<8u),1,0,0,0,0};
            vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[stage]);
            vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
            vkCmdDispatch(p.command,1,1,1);
            VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
            vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
        }
        host_barrier(&p,1);ASSERT_TRUE(vkEndCommandBuffer(p.command)==VK_SUCCESS&&submit(&p,"batched coupled grains",1));
        for(unsigned i=0;i<N-1;i++) {
            if(!batched) memcpy(expected[i],mapped[i],sizes[i]);
            else exact=exact&&memcmp(expected[i],mapped[i],sizes[i])==0;
        }
        if(!batched) changed=memcmp(initial[1],mapped[1],sizes[1])!=0&&
            ((dc_gpu_body_record_t *)mapped[0])->body.x_fp!=body->body.x_fp;
        for(unsigned i=0;i<(batched?7u:6u);i++) vkDestroyPipeline(p.device,pipelines[i],NULL);
    }
    else for(unsigned velocity=0;velocity<2;velocity++) for(unsigned fused=0;fused<2;fused++) {
        for(unsigned i=0;i<N;i++) memcpy(mapped[i],initial[i],sizes[i]);
        uint32_t phase=velocity?10u:7u;VkPipeline pipelines[]={fusion_pipeline(&p,&gpu,phase,fused),fusion_pipeline(&p,&gpu,velocity?17u:16u,false)};
        ASSERT_TRUE(pipelines[0]&&pipelines[1]&&begin(&p));
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
        for(unsigned stage=0;stage<(fused?1u:2u);stage++) {
            uint32_t push[8]={128,128,(stage?(velocity?17u:16u):phase)|(1u<<8u),1,0,0,0,0};
            vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[stage]);
            vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
            vkCmdDispatch(p.command,1,1,1);
            VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
            vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
        }
        host_barrier(&p,1);ASSERT_TRUE(vkEndCommandBuffer(p.command)==VK_SUCCESS&&submit(&p,"fused grain application",1));
        for(unsigned i=0;i<N-1;i++) {
            if(!fused) memcpy(expected[i],mapped[i],sizes[i]);
            else exact=exact&&memcmp(expected[i],mapped[i],sizes[i])==0;
        }
        if(!fused) changed=changed||memcmp(initial[1],mapped[1],sizes[1])!=0;
        for(unsigned i=0;i<2;i++) vkDestroyPipeline(p.device,pipelines[i],NULL);
    }
    fixture_release_pipeline(&p);
    for(unsigned i=0;i<N;i++) {
        vkUnmapMemory(p.device,memory[i]);vkDestroyBuffer(p.device,buffers[i],NULL);vkFreeMemory(p.device,memory[i],NULL);
        free(initial[i]);free(expected[i]);
    }
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);
    vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(changed);ASSERT_TRUE(exact);PASS();
}
static void test_fused_body_and_grain_application_preserves_every_byte(void) {run_grain_case(false);}
static void test_batched_jacobi_preserves_coupled_grains(void) {run_grain_case(true);}
int main(void) {RUN(test_fused_body_and_grain_application_preserves_every_byte);
    RUN(test_batched_jacobi_preserves_coupled_grains);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
