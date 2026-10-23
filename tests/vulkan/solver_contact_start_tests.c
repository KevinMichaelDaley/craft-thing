#define DC_SOLVER_MOTION_TEST_MAIN solver_motion_fixture_main
#include "solver_motion_store_tests.c"
#undef DC_SOLVER_MOTION_TEST_MAIN

static VkPipeline contact_start_pipeline(probe_t *p,dc_gpu_t *gpu,uint32_t phase,bool cached) {
    VkShaderModule module=VK_NULL_HANDLE;char err[256];
    if(!dc_gpu_load_shader_module(gpu,"build/diagnostics/solver_motion_profile.comp.spv",&module,err,sizeof(err))) return VK_NULL_HANDLE;
    uint32_t values[]={phase,cached};
    VkSpecializationMapEntry entries[]={{0,0,4},{3,4,4}};
    VkSpecializationInfo spec={2,entries,sizeof(values),values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=p->layout};
    VkPipeline result=VK_NULL_HANDLE;
    vkCreateComputePipelines(p->device,VK_NULL_HANDLE,1,&info,NULL,&result);
    vkDestroyShaderModule(p->device,module,NULL);return result;
}
static void test_cached_start_anchors_preserve_all_solver_and_body_bits(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical};
    enum {WIDTH=128,HEIGHT=128,RECORDS=64,CONTACT_BASE=8+32+4096*8,
          SOLVER_BASE=CONTACT_BASE+12+8192*32,BODY_STATE=SOLVER_BASE+16,
          CONTACT_STATE=BODY_STATE+64*32};
    VkDeviceSize bytes=dc_gpu_body_storage_bytes(&gpu);
    VkBuffer storage,profile;VkDeviceMemory storage_memory,profile_memory;uint32_t *mapped,*counts;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,bytes,&storage,&storage_memory,(void **)&mapped,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,16,&profile,&profile_memory,(void **)&counts,err,sizeof(err)));
    uint8_t *initial=calloc(1,(size_t)bytes),*expected=malloc((size_t)bytes);ASSERT_TRUE(initial&&expected);
    dc_gpu_body_record_t *b=(void *)initial;
    uint32_t *data=(void *)(initial+RECORDS*sizeof(*b));
    for(unsigned i=0;i<2;i++) {
        b[i].body=(dc_gpu_body_t){.x_fp=(int32_t)(20-i)*65536,.y_fp=(int32_t)(20+i)*65536,
            .vx_fp=1234,.vy_fp=-4321,.width=3+i,.height=7-i,.id=123+i,.active=1};
        b[i].visible=1;b[i].center[0]=1.5f+.5f*i;b[i].center[1]=3.5f-.5f*i;
        b[i].angle=i?-.21f:.13f;b[i].angular_velocity=.017f;b[i].radius=4;
        b[i].inverse_mass=1.f/(i+1.f);b[i].inverse_inertia=.12f;
        uint32_t state=BODY_STATE+32*i;
        data[state+13]=1;data[state+6]=float_bits(19.9f-i);data[state+7]=float_bits(19.9f+i);
        data[state+8]=float_bits(b[i].angle-.01f);
    }
    data[CONTACT_BASE]=2;
    for(unsigned i=0;i<2;i++) {
        uint32_t c=CONTACT_BASE+12+i*32;
        data[c]=123;data[c+1]=i?0:124;data[c+2]=i?1:0;
        data[c+8]=float_bits(0);data[c+9]=float_bits(-1);
        data[c+11]=float_bits(.5f);data[c+12]=float_bits(.1f);data[c+13]=float_bits(.00001f);
        data[c+20]=(uint32_t)((21.25f+i*.5f)*65536.f);
        data[c+21]=(uint32_t)((22.5f+i)*65536.f);
        data[c+26]=data[c+20];data[c+27]=data[c+21]-16384u;
    }
    p.buffer=storage;p.mapped=mapped;
    const uint32_t bindings[]={1,2,3,4,10,19,20,21,23,24,25,27,31};
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,storage,"build/diagnostics/solver_motion_profile.comp.spv",bindings,13,4));
    for(unsigned i=0;i<13;i++) {
        VkDescriptorBufferInfo buffer={i==12?profile:storage,0,i==12?16:bytes};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
        vkUpdateDescriptorSets(p.device,1,&write,0,NULL);
    }
    uint32_t rotations[2]={0};bool exact=false;
    for(unsigned cached=0;cached<2;cached++) {
        const uint32_t phases[]={4,6,7,9,10};VkPipeline pipelines[5];
        for(unsigned i=0;i<5;i++) {pipelines[i]=contact_start_pipeline(&p,&gpu,phases[i],cached);ASSERT_TRUE(pipelines[i]);}
        memcpy(mapped,initial,(size_t)bytes);memset(counts,0,16);ASSERT_TRUE(begin(&p));
        vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
        for(unsigned pass=0;pass<19;pass++) {
            unsigned stage=pass==0?0:pass<=16?1+(pass-1)%2:pass-14;
            uint32_t push[8]={WIDTH,HEIGHT,phases[stage],2,0,0,0,0};
            vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[stage]);
            vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
            vkCmdDispatch(p.command,1,1,1);
            VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
            vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,NULL,0,NULL);
        }
        host_barrier(&p,1);ASSERT_TRUE(vkEndCommandBuffer(p.command)==VK_SUCCESS&&submit(&p,"cached contact anchors",1));
        rotations[cached]=counts[1];
        if(!cached) memcpy(expected,mapped,(size_t)bytes);else exact=memcmp(expected,mapped,(size_t)bytes)==0;
        for(unsigned i=0;i<5;i++) vkDestroyPipeline(p.device,pipelines[i],NULL);
    }
    printf("exact body/contact solver bytes%d, anchor rotations%u -> %u\n",exact,rotations[0],rotations[1]);
    fixture_release_pipeline(&p);
    vkUnmapMemory(p.device,storage_memory);vkUnmapMemory(p.device,profile_memory);
    vkDestroyBuffer(p.device,storage,NULL);vkDestroyBuffer(p.device,profile,NULL);
    vkFreeMemory(p.device,storage_memory,NULL);vkFreeMemory(p.device,profile_memory,NULL);
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);
    vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);free(initial);free(expected);
    ASSERT_TRUE(exact);ASSERT_TRUE(rotations[0]>0u);ASSERT_TRUE(rotations[1]*2u<rotations[0]);PASS();
}
int main(void) {RUN(test_cached_start_anchors_preserve_all_solver_and_body_bits);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
