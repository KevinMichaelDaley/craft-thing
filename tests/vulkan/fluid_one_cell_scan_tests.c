#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"
#include "../../src/vulkan/subgroup_gpu.h"

static bool scan_override_present(uint32_t id) {
    FILE *file=fopen("build/shaders/fluid_transport.comp.spv","rb");if(!file) return false;
    bool okay=fseek(file,0,SEEK_END)==0;long bytes=okay?ftell(file):0;rewind(file);
    uint32_t *words=bytes>0?malloc((size_t)bytes):NULL;
    okay=words&&fread(words,1,(size_t)bytes,file)==(size_t)bytes;fclose(file);
    bool found=false;
    for(size_t i=5;okay&&i<(size_t)bytes/4;) {
        uint32_t length=words[i]>>16;if(!length||i+length>(size_t)bytes/4) break;
        if((words[i]&65535u)==71u&&length==4u&&words[i+2]==1u&&words[i+3]==id) found=true;
        i+=length;
    }
    free(words);return found;
}
static VkPipeline scan_pipeline(fixture_t *f,unsigned mode,bool enabled) {
    uint32_t values[]={mode,1u,enabled,enabled,enabled,enabled};
    VkSpecializationMapEntry entries[]={{0,0,4},{2,4,4},{5,8,4},{6,12,4},{7,16,4},{8,20,4}};
    VkSpecializationInfo spec={6,entries,sizeof(values),values};
    VkShaderModule module=VK_NULL_HANDLE;char err[256];
    if(!dc_gpu_load_shader_module(&f->gpu,enabled?"build/diagnostics/fluid_transport_profile_16.comp.spv":
        "build/diagnostics/fluid_transport_profile.comp.spv",&module,err,sizeof(err))) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=f->p.layout};
    uint32_t required_size=enabled?dc_gpu_transport_subgroup_size(f->p.physical):0u;
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo required={
        .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
        .requiredSubgroupSize=required_size};
    if(required_size) info.stage.pNext=&required;
    VkPipeline result=VK_NULL_HANDLE;
    VkResult status=vkCreateComputePipelines(f->p.device,VK_NULL_HANDLE,1,&info,NULL,&result);
    vkDestroyShaderModule(f->p.device,module,NULL);
    return status==VK_SUCCESS?result:VK_NULL_HANDLE;
}
static bool scan_submit(fixture_t *f,unsigned mode) {
    if(!begin(&f->p)) return false;
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,mode,0,0,bits(1.f),1,0};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,mode<2?WIDTH:HEIGHT,1,1);host_barrier(&f->p,1);
    return vkEndCommandBuffer(f->p.command)==VK_SUCCESS&&submit(&f->p,"one-cell ordered scan",1);
}
static void test_scan_matches_ordered_mass_and_pigment_for_every_direction(void) {
    ASSERT_TRUE(scan_override_present(5u));
    ASSERT_TRUE(scan_override_present(6u));
    ASSERT_TRUE(scan_override_present(7u));
    ASSERT_TRUE(scan_override_present(8u));
    for(unsigned mode=0;mode<4;mode++) for(unsigned variant=0;variant<14;variant++) {
        fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f&&initialize(&f->p));
        f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
        for(unsigned i=0;i<8;i++) {
            ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
            memset(f->words[i],0,sizes[i]*4);
        }
        for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=(variant==1||variant==10)&&i==1?0:i+1;
        for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
            unsigned cell=y*WIDTH+x,a=atlas(f,(int)x,(int)y),hash=x*7919u+y*17239u;
            f->words[2][2*CELLS+cell]=(hash%4)*FULL/4;
            float speed=(mode&1u?-1.f:1.f)*((hash%7)+1)*.1f;
            f->words[4][2*cell+(mode<2?1:0)]=bits(speed);f->words[3][cell]=bits(speed*.5f);
            if(a==UINT32_MAX) continue;
            f->words[0][2*a]=hash%29==0?1u:hash%17==0?4u:0u;
            f->words[0][2*a+1]=minimum(hash%(FULL+1),capacity(f,(int)x,(int)y));
            if(variant>=4) {
                f->words[2][2*CELLS+cell]=0u;
                f->words[0][2*a]=0u;
                unsigned coordinate=mode<2?y:x;
                bool slab=(mode&1u)?coordinate>=25u:coordinate<50u;
                f->words[0][2*a+1]=variant==5?(hash%23u==0u?0u:FULL):
                    !slab?0u:(variant==6||variant==7||variant==8||variant==10||variant==11)?hash%(FULL+1u):FULL;
                float packed_speed=(mode&1u?-1.f:1.f)*(variant==4?3.f:variant==5?((hash%7u)+1u)*.6f:variant==7?1.37f:(variant==8||variant==10)?21.3f:2.37f);
                f->words[4][2*cell+(mode<2?1:0)]=bits(packed_speed);
                f->words[3][cell]=bits(packed_speed);
                if(variant==9&&coordinate==((mode&1u)?23u:51u)) {
                    f->words[0][2*a+1]=FULL/2u;
                    f->words[4][2*cell+(mode<2?1:0)]=bits(0.f);f->words[3][cell]=bits(0.f);
                }
                if(variant==9&&coordinate==((mode&1u)?22u:52u)) {
                    f->words[0][2*a+1]=FULL*3u/4u;
                    f->words[4][2*cell+(mode<2?1:0)]=bits(0.f);f->words[3][cell]=bits(0.f);
                }
                if(variant==11&&coordinate==37u) {f->words[0][2*a]=1u;f->words[0][2*a+1]=0u;}
                if(variant==11&&coordinate==40u) {
                    f->words[2][2*CELLS+cell]=FULL/2u;
                    f->words[2][(mode<2?4u:3u)*CELLS+cell]=FULL/2u;
                    f->words[0][2*a+1]=minimum(f->words[0][2*a+1],FULL/2u);
                }
            }
            float *color=(float *)(f->words[7]+4*a);
            color[0]=(hash%251)/251.f;color[1]=((hash/7)%239)/239.f;color[2]=((hash/19)%233)/233.f;color[3]=1.f;
        }
        if(variant>=12) for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
            if(variant==13) {
                float speed=(mode&1u?-0.2f:0.2f);
                unsigned cell=y*WIDTH+x;
                f->words[4][2*cell+(mode<2?1:0)]=bits(speed);f->words[3][cell]=bits(speed);
            }
            uint32_t a=atlas(f,x,y),coordinate=mode<2?y:x,length=mode<2?HEIGHT:WIDTH;
            if(a!=UINT32_MAX) {
                f->words[0][2*a]=0u;
                f->words[0][2*a+1]=((mode&1u)?coordinate+8u<length:coordinate>=8u)?FULL:0u;
            }
        }
        unsigned spot=atlas(f,4,4);
        if(variant==2) f->words[0][2*spot+1]=FULL*2;
        if(variant==3) f->words[4][2*index_of(4,4)+(mode<2?1:0)]=bits((mode&1u?-1.f:1.f)*3.f);
        size_t mass_bytes=sizes[0]*4,color_bytes=sizes[7]*4;
        void *initial_mass=malloc(mass_bytes),*initial_color=malloc(color_bytes),*reference_mass=malloc(mass_bytes),*reference_color=malloc(color_bytes);
        ASSERT_TRUE(initial_mass&&initial_color&&reference_mass&&reference_color);
        memcpy(initial_mass,f->words[0],mass_bytes);memcpy(initial_color,f->words[7],color_bytes);
        f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
        VkBuffer profile;VkDeviceMemory profile_memory;uint32_t *counts;
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,28u,&profile,&profile_memory,(void **)&counts,err,sizeof(err)));
        memset(counts,0,28u);
        uint32_t descriptors[9];memcpy(descriptors,bindings,sizeof(bindings));descriptors[8]=31u;
        ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,"build/diagnostics/fluid_transport_profile.comp.spv",descriptors,9,mode));
        VkDescriptorBufferInfo buffers[9];VkWriteDescriptorSet writes[9];
        for(unsigned i=0;i<9;i++) {
            buffers[i]=(VkDescriptorBufferInfo){i==8?profile:f->buffers[i],0,i==8?28u:sizes[i]*4};
            writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
                .dstBinding=descriptors[i],.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=buffers+i};
        }
        vkUpdateDescriptorSets(f->p.device,9,writes,0,NULL);
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=scan_pipeline(f,mode,false);ASSERT_TRUE(f->p.pipeline);
        ASSERT_TRUE(scan_submit(f,mode));memcpy(reference_mass,f->words[0],mass_bytes);memcpy(reference_color,f->words[7],color_bytes);
        uint32_t reference_work=counts[2];memset(counts,0,28u);
        memcpy(f->words[0],initial_mass,mass_bytes);memcpy(f->words[7],initial_color,color_bytes);
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=scan_pipeline(f,mode,true);ASSERT_TRUE(f->p.pipeline);
        ASSERT_TRUE(scan_submit(f,mode));
        bool exact_mass=memcmp(reference_mass,f->words[0],mass_bytes)==0,exact_color=memcmp(reference_color,f->words[7],color_bytes)==0;
        bool stagnant=(variant!=12u||counts[6]>0u)&&(variant!=13u||counts[4]==0u);
        printf("parallel stationary sources%u\n",counts[6]);
        bool cached_pages=counts[5]==0u;
        bool subgroup_size=!dc_gpu_transport_subgroup_size(f->p.physical)||counts[3]==16u;
        printf("uncached runtime page reads%u\n",counts[5]);
        bool parallel=(variant!=6&&variant!=7&&variant!=8)||counts[2]*4u<reference_work;
        printf("mode%u variant%u: exact mass%d pigment%d, ordered sources%u -> %u\n",
            mode,variant,exact_mass,exact_color,reference_work,counts[2]);
        if(!exact_mass) {
            unsigned printed=0;uint32_t *expected=reference_mass;
            for(unsigned word=0;word<sizes[0]&&printed<8;word++) if(expected[word]!=f->words[0][word]) {
                printf("mass word%u expected%u actual%u\n",word,expected[word],f->words[0][word]);printed++;
            }
        }
        fixture_release_pipeline(&f->p);
        vkUnmapMemory(f->p.device,profile_memory);vkDestroyBuffer(f->p.device,profile,NULL);vkFreeMemory(f->p.device,profile_memory,NULL);
        for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
        vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
        vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);
        free(initial_mass);free(initial_color);free(reference_mass);free(reference_color);free(f);
        ASSERT_TRUE(exact_mass);ASSERT_TRUE(exact_color);ASSERT_TRUE(parallel);ASSERT_TRUE(cached_pages);ASSERT_TRUE(subgroup_size);ASSERT_TRUE(stagnant);
    }
    PASS();
}
int main(void) {
    RUN(test_scan_matches_ordered_mass_and_pigment_for_every_direction);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
