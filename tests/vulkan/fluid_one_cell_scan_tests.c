#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"

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
    uint32_t values[]={mode,1u,enabled};
    VkSpecializationMapEntry entries[]={{0,0,4},{2,4,4},{5,8,4}};
    VkSpecializationInfo spec={3,entries,sizeof(values),values};
    VkShaderModule module=VK_NULL_HANDLE;char err[256];
    if(!dc_gpu_load_shader_module(&f->gpu,enabled?"build/shaders/fluid_transport_16.comp.spv":
        "build/shaders/fluid_transport.comp.spv",&module,err,sizeof(err))) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=f->p.layout};
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
    for(unsigned mode=0;mode<4;mode++) for(unsigned variant=0;variant<6;variant++) {
        fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f&&initialize(&f->p));
        f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
        for(unsigned i=0;i<8;i++) {
            ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
            memset(f->words[i],0,sizes[i]*4);
        }
        for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=variant==1&&i==1?0:i+1;
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
                f->words[0][2*a+1]=(variant==4?coordinate==50u:hash%23u==0u)?0u:FULL;
                float packed_speed=(mode&1u?-1.f:1.f)*(variant==4?3.f:((hash%7u)+1u)*.6f);
                f->words[4][2*cell+(mode<2?1:0)]=bits(packed_speed);
                f->words[3][cell]=bits(packed_speed);
            }
            float *color=(float *)(f->words[7]+4*a);
            color[0]=(hash%251)/251.f;color[1]=((hash/7)%239)/239.f;color[2]=((hash/19)%233)/233.f;color[3]=1.f;
        }
        unsigned spot=atlas(f,4,4);
        if(variant==2) f->words[0][2*spot+1]=FULL*2;
        if(variant==3) f->words[4][2*index_of(4,4)+(mode<2?1:0)]=bits((mode&1u?-1.f:1.f)*3.f);
        size_t mass_bytes=sizes[0]*4,color_bytes=sizes[7]*4;
        void *initial_mass=malloc(mass_bytes),*initial_color=malloc(color_bytes),*reference_mass=malloc(mass_bytes),*reference_color=malloc(color_bytes);
        ASSERT_TRUE(initial_mass&&initial_color&&reference_mass&&reference_color);
        memcpy(initial_mass,f->words[0],mass_bytes);memcpy(initial_color,f->words[7],color_bytes);
        f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
        ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,"build/shaders/fluid_transport.comp.spv",bindings,8,mode));
        VkDescriptorBufferInfo buffers[8];VkWriteDescriptorSet writes[8];
        for(unsigned i=0;i<8;i++) {
            buffers[i]=(VkDescriptorBufferInfo){f->buffers[i],0,sizes[i]*4};
            writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
                .dstBinding=bindings[i],.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=buffers+i};
        }
        vkUpdateDescriptorSets(f->p.device,8,writes,0,NULL);
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=scan_pipeline(f,mode,false);ASSERT_TRUE(f->p.pipeline);
        ASSERT_TRUE(scan_submit(f,mode));memcpy(reference_mass,f->words[0],mass_bytes);memcpy(reference_color,f->words[7],color_bytes);
        memcpy(f->words[0],initial_mass,mass_bytes);memcpy(f->words[7],initial_color,color_bytes);
        vkDestroyPipeline(f->p.device,f->p.pipeline,NULL);f->p.pipeline=scan_pipeline(f,mode,true);ASSERT_TRUE(f->p.pipeline);
        ASSERT_TRUE(scan_submit(f,mode));
        bool exact_mass=memcmp(reference_mass,f->words[0],mass_bytes)==0,exact_color=memcmp(reference_color,f->words[7],color_bytes)==0;
        printf("mode%u variant%u: exact mass%d pigment%d\n",mode,variant,exact_mass,exact_color);
        if(!exact_mass) {
            unsigned printed=0;uint32_t *expected=reference_mass;
            for(unsigned word=0;word<sizes[0]&&printed<8;word++) if(expected[word]!=f->words[0][word]) {
                printf("mass word%u expected%u actual%u\n",word,expected[word],f->words[0][word]);printed++;
            }
        }
        fixture_release_pipeline(&f->p);
        for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
        vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
        vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);
        free(initial_mass);free(initial_color);free(reference_mass);free(reference_color);free(f);
        ASSERT_TRUE(exact_mass);ASSERT_TRUE(exact_color);
    }
    PASS();
}
int main(void) {
    RUN(test_scan_matches_ordered_mass_and_pigment_for_every_direction);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
