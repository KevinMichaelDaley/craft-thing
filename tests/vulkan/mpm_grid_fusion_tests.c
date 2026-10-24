#define main grid_transfer_fixture_main
#include "mpm_gather_tests.c"
#undef main

static bool grid_submit(dc_gpu_t *g,VkPipeline pipeline,unsigned mode) {
    if(!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,mode};
    vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,(WIDTH+15u)/16u,(HEIGHT+15u)/16u,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g);
}

static VkPipeline grid_pipeline(dc_gpu_t *g,VkShaderModule module,bool fused) {
    uint32_t value=fused;VkSpecializationMapEntry entry={4,0,4};
    VkSpecializationInfo spec={1,&entry,sizeof(value),&value};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline result=VK_NULL_HANDLE;
    vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&result);return result;
}

static void test_fused_node_transfers_preserve_grid_force_velocity_and_water_feedback(void) {
    dc_gpu_t *g=NULL;char err[256];
    ASSERT_TRUE(dc_gpu_create(&g,WIDTH,HEIGHT,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    enum {CELLS=WIDTH*HEIGHT,N=8};
    const uint32_t bindings[N]={1,19,23,24,25,27,10,20};
    const VkDeviceSize sizes[N]={CHUNKS*4096u*8u,PARTICLES*sizeof(dc_mpm_particle_t),
        CELLS*16u,CELLS*16u,CELLS*8u,(11u+2u*TILES)*4u,CELLS*8u,CHUNKS*4u};
    test_buffer_t b[N]={0};void *initial[N],*expected[N];
    for(unsigned i=0;i<N;i++) {
        b[i].bytes=sizes[i];initial[i]=calloc(1,(size_t)sizes[i]);expected[i]=malloc((size_t)sizes[i]);
        ASSERT_TRUE(initial[i]&&expected[i]);
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,sizes[i],&b[i].buffer,&b[i].memory,&b[i].mapped,err,sizeof(err)));
        VkDescriptorBufferInfo buffer={b[i].buffer,0,sizes[i]};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
        vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    }
    uint32_t *activity=initial[5];activity[10u+2u*TILES]=TILES;
    for(unsigned i=0;i<TILES;i++) activity[3u+i]=i;
    for(unsigned slot=0;slot<CHUNKS;slot++) ((uint32_t *)initial[7])[slot]=8192u;
    VkShaderModule module=VK_NULL_HANDLE;
    ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/shaders/mpm.comp.spv",&module,err,sizeof(err)));
    VkPipeline pipeline[]={grid_pipeline(g,module,false),grid_pipeline(g,module,true)};
    ASSERT_TRUE(pipeline[0]&&pipeline[1]);vkDestroyShaderModule(g->device,module,NULL);
    for(unsigned variant=0;variant<4;variant++) {
        for(unsigned slot=0;slot<CHUNKS;slot++) ((uint32_t *)g->page_mapped)[slot]=variant==3&&slot==1?0u:slot+1u;
        for(unsigned i=0;i<CHUNKS*4096u;i++) {
            unsigned hash=i*7919u,local=i%4096u;
            ((uint32_t *)initial[0])[2u*i]=variant==2&&hash%7u==0u?DC_MATERIAL_STONE:DC_MATERIAL_DIRT;
            ((uint32_t *)initial[0])[2u*i+1u]=variant?hash%65537u:0u;
            for(unsigned layer=0;layer<2;layer++) {
                unsigned index=(i/4096u)*8192u+layer*4096u+local;
                ((dc_mpm_particle_t *)initial[1])[index]=(dc_mpm_particle_t){
                    .x_fp=(int32_t)(local%64u)*65536+8192+(int32_t)(hash%7u)*8192,
                    .y_fp=(int32_t)(local/64u)*65536+8192+(int32_t)((hash/7u)%7u)*8192,
                    .vx_fp=(int32_t)(hash%40001u)-20000,.vy_fp=(int32_t)((hash/3u)%40001u)-20000,
                    .deformation={1.f,.125f,-.25f,.875f},.id_lo=hash,.id_hi=layer,
                    .mass_fp=layer?(variant&&hash%3u==0u?16384u:0u):65536u,
                    .grain_fp=32768u,.material=DC_MATERIAL_DIRT,.flags=hash%16385u};
            }
        }
        for(unsigned i=0;i<CELLS;i++) {
            ((float *)initial[6])[2*i]=((int)(i%17u)-8)/16.f;
            ((float *)initial[6])[2*i+1]=((int)(i%19u)-9)/16.f;
        }
        bool exact=true;
        for(unsigned fused=0;fused<2;fused++) {
            for(unsigned i=0;i<N;i++) memcpy(b[i].mapped,initial[i],(size_t)sizes[i]);
            ASSERT_TRUE(grid_submit(g,pipeline[fused],0));
            if(!fused) {
                ASSERT_TRUE(grid_submit(g,pipeline[fused],1));
                ASSERT_TRUE(grid_submit(g,pipeline[fused],6));
            }
            for(unsigned i=0;i<N;i++) {
                if(!fused) memcpy(expected[i],b[i].mapped,(size_t)sizes[i]);
                else if(memcmp(expected[i],b[i].mapped,(size_t)sizes[i])!=0) {
                    printf("fused grid variant%u differs binding%u\n",variant,bindings[i]);exact=false;
                }
            }
        }
        printf("fused grid variant%u all node and particle state exact%d\n",variant,exact);
        ASSERT_TRUE(exact);
    }
    for(unsigned i=0;i<2;i++) vkDestroyPipeline(g->device,pipeline[i],NULL);
    for(unsigned i=0;i<N;i++) {
        vkUnmapMemory(g->device,b[i].memory);vkDestroyBuffer(g->device,b[i].buffer,NULL);vkFreeMemory(g->device,b[i].memory,NULL);
        free(initial[i]);free(expected[i]);
    }
    dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_fused_node_transfers_preserve_grid_force_velocity_and_water_feedback);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
