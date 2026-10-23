#define main transfer_gather_fixture_main
#include "mpm_gather_tests.c"
#undef main

static VkPipeline transfer_pipeline(dc_gpu_t *g,VkShaderModule module,bool early) {
    uint32_t value=early;
    VkSpecializationMapEntry entry={3,0,4};
    VkSpecializationInfo spec={1,&entry,sizeof(value),&value};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline result=VK_NULL_HANDLE;
    vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&result);return result;
}
static bool transfer_submit(dc_gpu_t *g,VkPipeline pipeline,unsigned mode) {
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
static void test_transfer_prunes_unused_records_with_identical_grid_force_and_proposals(void) {
    dc_gpu_t *g=NULL;char err[256];
    ASSERT_TRUE(dc_gpu_create(&g,WIDTH,HEIGHT,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned slot=0;slot<CHUNKS;slot++) {
        chunk->coord=(dc_chunk_coord_t){slot%PAGE_COLUMNS,slot/PAGE_COLUMNS};
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,slot%PAGE_COLUMNS,slot/PAGE_COLUMNS,slot,err,sizeof(err)));
    }
    free(chunk);
    enum {CELLS=WIDTH*HEIGHT};
    const uint32_t bindings[]={1,19,21,23,24,25,27,31,20};
    const VkDeviceSize sizes[]={CHUNKS*4096u*8u,PARTICLES*sizeof(dc_mpm_particle_t),
        PARTICLES*sizeof(dc_mpm_particle_t),CELLS*16u,CELLS*16u,CELLS*8u,(11u+2u*TILES)*4u,16u,CHUNKS*4u};
    test_buffer_t b[9]={0};
    for(unsigned i=0;i<9;i++) {
        b[i].bytes=sizes[i];
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,sizes[i],&b[i].buffer,&b[i].memory,&b[i].mapped,err,sizeof(err)));
        memset(b[i].mapped,0,(size_t)sizes[i]);
        VkDescriptorBufferInfo buffer={b[i].buffer,0,sizes[i]};
        VkWriteDescriptorSet write={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
        vkUpdateDescriptorSets(g->device,1,&write,0,NULL);
    }
    uint32_t *activity=b[6].mapped;activity[10u+2u*TILES]=TILES;
    for(unsigned i=0;i<TILES;i++) activity[3u+i]=i;
    for(unsigned i=0;i<CHUNKS;i++) ((uint32_t *)b[8].mapped)[i]=8192u;
    for(unsigned i=0;i<CELLS;i++) {
        ((float *)b[5].mapped)[2u*i]=(float)((int)(i%17u)-8)/16.f;
        ((float *)b[5].mapped)[2u*i+1u]=(float)((int)(i%19u)-9)/16.f;
    }
    VkShaderModule module=VK_NULL_HANDLE;
    ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/diagnostics/mpm_gather_profile.comp.spv",&module,err,sizeof(err)));
    VkPipeline pipelines[]={transfer_pipeline(g,module,false),transfer_pipeline(g,module,true)};
    ASSERT_TRUE(pipelines[0]&&pipelines[1]);vkDestroyShaderModule(g->device,module,NULL);
    void *expected_grid=malloc((size_t)sizes[3]),*expected_force=malloc((size_t)sizes[4]);
    void *expected_proposals=malloc((size_t)sizes[2]);ASSERT_TRUE(expected_grid&&expected_force&&expected_proposals);
    for(unsigned variant=0;variant<4;variant++) {
        for(unsigned i=0;i<CHUNKS;i++) ((uint32_t *)g->page_mapped)[i]=variant==3&&i==1?0u:i+1u;
        dc_mpm_particle_t *particles=b[1].mapped;
        for(unsigned i=0;i<CHUNKS*4096u;i++) {
            unsigned hash=i*7919u,local=i%4096u;
            ((uint32_t *)b[0].mapped)[2u*i]=variant==2&&hash%3u==0u?DC_MATERIAL_STONE:DC_MATERIAL_DIRT;
            ((uint32_t *)b[0].mapped)[2u*i+1u]=hash%65537u;
            for(unsigned layer=0;layer<2;layer++) {
                unsigned index=(i/4096u)*8192u+layer*4096u+local;
                particles[index]=(dc_mpm_particle_t){.x_fp=(int32_t)(local%64u)*65536+
                        (variant?8192+(int32_t)(hash%7u)*8192:32768),
                    .y_fp=(int32_t)(local/64u)*65536+(variant?8192+(int32_t)((hash/7u)%7u)*8192:32768),
                    .vx_fp=(int32_t)(hash%40001u)-20000,.vy_fp=(int32_t)((hash/3u)%40001u)-20000,
                    .deformation={1.f,.125f,-.25f,.875f},.id_lo=hash,.id_hi=layer,
                    .mass_fp=layer?(variant&&hash%3u==0u?16384u:0u):65536u,.grain_fp=32768u,
                    .material=DC_MATERIAL_DIRT,.flags=hash%16385u};
            }
        }
        uint32_t reads[2]={0};bool exact_grid=false,exact_proposals=false;
        for(unsigned early=0;early<2;early++) {
            memset(b[3].mapped,0xcd,(size_t)sizes[3]);memset(b[4].mapped,0xcd,(size_t)sizes[4]);
            memset(b[7].mapped,0,16);ASSERT_TRUE(transfer_submit(g,pipelines[early],0));
            reads[early]=((uint32_t *)b[7].mapped)[1];
            if(!early) {memcpy(expected_grid,b[3].mapped,(size_t)sizes[3]);memcpy(expected_force,b[4].mapped,(size_t)sizes[4]);}
            else exact_grid=memcmp(expected_grid,b[3].mapped,(size_t)sizes[3])==0&&memcmp(expected_force,b[4].mapped,(size_t)sizes[4])==0;
            memset(b[2].mapped,0xcd,(size_t)sizes[2]);ASSERT_TRUE(transfer_submit(g,pipelines[early],2));
            if(!early) memcpy(expected_proposals,b[2].mapped,(size_t)sizes[2]);
            else exact_proposals=memcmp(expected_proposals,b[2].mapped,(size_t)sizes[2])==0;
        }
        printf("variant%u exact grid/force%d proposals%d record loads%u -> %u\n",variant,exact_grid,exact_proposals,reads[0],reads[1]);
        ASSERT_TRUE(exact_grid);ASSERT_TRUE(exact_proposals);ASSERT_TRUE(reads[0]>0u&&reads[1]*2u<reads[0]);
    }
    free(expected_grid);free(expected_force);free(expected_proposals);
    for(unsigned i=0;i<2;i++) vkDestroyPipeline(g->device,pipelines[i],NULL);
    for(unsigned i=0;i<9;i++) {
        vkUnmapMemory(g->device,b[i].memory);vkDestroyBuffer(g->device,b[i].buffer,NULL);vkFreeMemory(g->device,b[i].memory,NULL);
    }
    dc_gpu_destroy(g);PASS();
}
int main(void) {RUN(test_transfer_prunes_unused_records_with_identical_grid_force_and_proposals);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
