#define FLUID_TRANSPORT_FIXTURE_ONLY
#define DC_FIXTURE_WATER_COLOR
#include "fluid_transport_tests.c"

#undef RUN
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)

static void water_case(bool soil,bool displacement,bool distant) {
    fixture_t *f=calloc(1,sizeof(*f));
    ASSERT_TRUE(f && initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};
    char err[256];
    for(unsigned i=0;i<7;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void**)&f->words[i],err,sizeof(err)));
        memset(f->words[i],0,sizes[i]*4);
    }
    VkBuffer pigment;VkDeviceMemory pigment_memory;float *color;
    const unsigned pigment_bytes=CHUNKS*4096*16;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,pigment_bytes,&pigment,&pigment_memory,(void**)&color,err,sizeof(err)));
    memset(color,0,pigment_bytes);
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=i+1;
    unsigned sy=displacement?64:distant?62:63,ty=displacement?63:64;
    uint32_t source=atlas(f,63,sy),target=atlas(f,63,ty),face=index_of(63,sy);
    f->words[0][source*2+1]=FULL;
    f->words[0][target*2]=soil?4u:0u;
    uint32_t initial=soil?0u:distant?FULL/4:FULL/2;
    f->words[0][target*2+1]=initial;
    color[source*4]=1;color[source*4+3]=1;
    color[target*4+2]=1;color[target*4+3]=1;
    if(displacement) {f->words[2][index_of(63,sy)]=1;f->words[2][2*CELLS+index_of(63,sy)]=FULL/2;}
    if(distant) {unsigned middle=atlas(f,63,63);f->words[0][2*middle+1]=FULL;color[4*middle+2]=1;color[4*middle+3]=1;}
    float speed=(distant?1.5f:.5f)/(TRANSPORT_TEST_STEP*1.001f);
    f->words[4][2*face+1]=bits(speed);f->words[3][face]=bits(speed);
    uint32_t color_bindings[8];memcpy(color_bindings,bindings,sizeof(bindings));color_bindings[7]=32;
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,displacement?
        "build/shaders/fluid_displacement.comp.spv":"build/shaders/fluid_transport.comp.spv",color_bindings,8,0));
    VkDescriptorBufferInfo info[8];VkWriteDescriptorSet writes[8];
    for(unsigned i=0;i<8;i++) {
        info[i]=(VkDescriptorBufferInfo){i<7?f->buffers[i]:pigment,0,i<7?sizes[i]*4:pigment_bytes};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet=f->p.descriptor,.dstBinding=color_bindings[i],.descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&info[i]};
    }
    vkUpdateDescriptorSets(f->p.device,8,writes,0,NULL);
    ASSERT_TRUE(begin(&f->p));
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,0,0,0,bits(TRANSPORT_TEST_STEP),1};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,WIDTH,1,1);host_barrier(&f->p,1);
    ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&f->p,"water pigment and soil conductivity",1));
    uint32_t remaining=f->words[0][2*source+1],received=f->words[0][2*target+1];
    float red=color[4*target],blue=color[4*target+2];
    bool conserved=remaining+received==FULL+initial;
    bool mixed=soil || (fabsf(red-(float)(received-initial)/received)<1e-5f &&
        fabsf(blue-(float)initial/received)<1e-5f &&
        fabsf(remaining*color[4*source]+received*red-FULL)<.1f);
    bool throttled=!soil || (received>0 && received<=1024);
    printf("soil=%u received=%u red=%f blue=%f\n",soil,received,red,blue);
    fixture_release_pipeline(&f->p);
    for(unsigned i=0;i<7;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkUnmapMemory(f->p.device,pigment_memory);vkDestroyBuffer(f->p.device,pigment,NULL);vkFreeMemory(f->p.device,pigment_memory,NULL);
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_TRUE(conserved);ASSERT_TRUE(mixed);ASSERT_TRUE(throttled);PASS();
}
static void test_water_colors_mix_by_mass_across_chunk_seam(void) {water_case(false,false,false);}
static void test_soil_throttles_water_without_losing_mass(void) {water_case(true,false,false);}
static void test_body_displacement_carries_and_mixes_pigment(void) {water_case(false,true,false);}
static void test_multicell_flow_carries_and_mixes_pigment(void) {water_case(false,false,true);}
int main(void) {
    RUN(test_water_colors_mix_by_mass_across_chunk_seam);
    RUN(test_soil_throttles_water_without_losing_mass);
    RUN(test_body_displacement_carries_and_mixes_pigment);
    RUN(test_multicell_flow_carries_and_mixes_pigment);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
