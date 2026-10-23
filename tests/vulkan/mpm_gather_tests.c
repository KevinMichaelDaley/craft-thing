#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/renderer_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
#ifndef DC_GATHER_TEST_WIDTH
#define DC_GATHER_TEST_WIDTH 128u
#define DC_GATHER_TEST_HEIGHT 64u
#endif
enum { WIDTH=DC_GATHER_TEST_WIDTH,HEIGHT=DC_GATHER_TEST_HEIGHT,
       PAGE_COLUMNS=(WIDTH+63u)/64u,CHUNKS=PAGE_COLUMNS*((HEIGHT+63u)/64u),
       TILES=((WIDTH+15u)/16u)*((HEIGHT+15u)/16u),PARTICLES=CHUNKS*8192u };
typedef struct {VkBuffer buffer;VkDeviceMemory memory;void *mapped;VkDeviceSize bytes;} test_buffer_t;

static bool gather_override_present(void) {
    FILE *file=fopen("build/shaders/mpm.comp.spv","rb");if(!file) return false;
    bool okay=fseek(file,0,SEEK_END)==0;long bytes=okay?ftell(file):0;rewind(file);
    uint32_t *words=bytes>0?malloc((size_t)bytes):NULL;
    okay=words&&fread(words,1,(size_t)bytes,file)==(size_t)bytes;fclose(file);bool found=false;
    for(size_t i=5;okay&&i<(size_t)bytes/4;) {
        uint32_t count=words[i]>>16;if(!count||i+count>(size_t)bytes/4) break;
        if((words[i]&65535u)==71u&&count==4u&&words[i+2]==1u&&words[i+3]==0u) found=true;
        i+=count;
    }
    free(words);return found;
}
static VkPipeline gather_pipeline(dc_gpu_t *g,VkShaderModule module,uint32_t enabled) {
    uint32_t values[]={enabled,enabled};
    VkSpecializationMapEntry entries[]={{0,0,4},{1,4,4}};
    VkSpecializationInfo spec={.mapEntryCount=2,.pMapEntries=entries,.dataSize=sizeof(values),.pData=values};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&spec},.layout=g->pipeline_layout};
    VkPipeline pipeline=VK_NULL_HANDLE;
    return vkCreateComputePipelines(g->device,VK_NULL_HANDLE,1,&info,NULL,&pipeline)==VK_SUCCESS?pipeline:VK_NULL_HANDLE;
}
static bool gather_submit(dc_gpu_t *g,VkPipeline pipeline) {
    if(!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,3};
    for(unsigned mode=3;mode<=4;mode++) {
        push[2]=mode;
        vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(g->command,(WIDTH+15u)/16u,(HEIGHT+15u)/16u,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);
    }
    return fr_renderer_submit(g);
}
static bool commit_submit(dc_gpu_t *g,VkPipeline pipeline) {
    if(!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,5};
    vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,(WIDTH+15u)/16u,(HEIGHT+15u)/16u,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g);
}
static void reset_commit_metadata(test_buffer_t *buffers) {
    uint32_t *cells=buffers[6].mapped,*counts=buffers[7].mapped;
    for(unsigned i=0;i<CHUNKS*4096u;i++) {cells[i*2u]=DC_MATERIAL_DIRT;cells[i*2u+1u]=i%65537u;}
    for(unsigned i=0;i<CHUNKS;i++) counts[i]=8192u;
}
static void test_stationary_and_moving_gathers_match_the_ordered_reference_exactly(void) {
    ASSERT_TRUE(gather_override_present());
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,WIDTH,HEIGHT,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned slot=0;slot<CHUNKS;slot++) {
        chunk->coord=(dc_chunk_coord_t){slot%PAGE_COLUMNS,slot/PAGE_COLUMNS};chunk->particle_count=4096;
        for(unsigned cell=0;cell<4096;cell++) {
            chunk->cells[cell].material=DC_MATERIAL_DIRT;
            dc_chunk_particle_init(chunk->particles+cell,chunk->coord,cell,DC_MATERIAL_DIRT);
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,slot,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,slot%PAGE_COLUMNS,slot/PAGE_COLUMNS,slot,err,sizeof(err)));
    }
    free(chunk);
    const uint32_t bindings[]={19,21,22,26,27,31,1,20};
    test_buffer_t buffer[8]={0};
    VkDescriptorBufferInfo info[8];VkWriteDescriptorSet writes[8];
    for(unsigned i=0;i<8;i++) {
        buffer[i].bytes=i<3?PARTICLES*sizeof(dc_mpm_particle_t):i==3?PARTICLES*4u:i==4?(11u+2u*TILES)*4u:i==5?16u:i==6?CHUNKS*4096u*8u:CHUNKS*4u;
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(g,buffer[i].bytes,&buffer[i].buffer,&buffer[i].memory,&buffer[i].mapped,err,sizeof(err)));
        memset(buffer[i].mapped,0,(size_t)buffer[i].bytes);
        info[i]=(VkDescriptorBufferInfo){buffer[i].buffer,0,buffer[i].bytes};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
            .dstBinding=bindings[i],.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=info+i};
    }
    vkUpdateDescriptorSets(g->device,8,writes,0,NULL);
    uint32_t *activity=buffer[4].mapped;activity[10+2*TILES]=TILES;
    for(unsigned i=0;i<TILES;i++) activity[3+i]=i;
    VkShaderModule module;ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/diagnostics/mpm_gather_profile.comp.spv",&module,err,sizeof(err)));
    VkPipeline reference=gather_pipeline(g,module,0),optimized=gather_pipeline(g,module,1);
    ASSERT_TRUE(reference&&optimized);vkDestroyShaderModule(g->device,module,NULL);
    ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/shaders/mpm.comp.spv",&module,err,sizeof(err)));
    VkPipeline scalar_commit=gather_pipeline(g,module,1);ASSERT_TRUE(scalar_commit);vkDestroyShaderModule(g->device,module,NULL);
    ASSERT_TRUE(dc_gpu_load_shader_module(g,"build/shaders/mpm_commit.comp.spv",&module,err,sizeof(err)));
    VkPipeline packed_commit=gather_pipeline(g,module,1);ASSERT_TRUE(packed_commit);vkDestroyShaderModule(g->device,module,NULL);
    void *expected=malloc((size_t)buffer[2].bytes);ASSERT_TRUE(expected);
    for(unsigned variant=0;variant<4;variant++) {
        reset_commit_metadata(buffer);
        dc_mpm_particle_t *old=buffer[0].mapped,*proposal=buffer[1].mapped;
        memset(old,0,(size_t)buffer[0].bytes);memset(proposal,0,(size_t)buffer[1].bytes);
        ((uint32_t *)g->page_mapped)[1]=variant==3?0u:2u;
        for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) for(unsigned layer=0;layer<2;layer++) {
            unsigned hash=x*7919u+y*17239u+layer*23u,index=((y/64)*PAGE_COLUMNS+x/64)*8192u+layer*4096u+(y%64u)*64u+x%64u;
            dc_mpm_particle_t p={.x_fp=(int32_t)(x%64)*65536+32768,.y_fp=(int32_t)(y%64u)*65536+32768,
                .vx_fp=(int32_t)(hash%123),.vy_fp=-(int32_t)(hash%177),
                .deformation={1.f,.125f,-.25f,.875f},
                .id_lo=hash%97u,.id_hi=(x+y)%11u,.mass_fp=layer?16384u:32768u,
                .grain_fp=32768u,.material=DC_MATERIAL_DIRT,.flags=hash};
            if((layer&&variant<2)||(hash%7u==0u)) p.mass_fp=0;
            old[index]=p;
            if(variant!=0) {
                p.x_fp+=((int32_t)(hash%3u)-1)*65536;
                p.y_fp+=((int32_t)((hash/3u)%3u)-1)*65536;
            }
            proposal[index]=p;
        }
        memset(buffer[2].mapped,0xcd,(size_t)buffer[2].bytes);memset(buffer[3].mapped,0,(size_t)buffer[3].bytes);
        memset(buffer[5].mapped,0,16u);
        ASSERT_TRUE(gather_submit(g,reference));
        uint32_t reference_reads=((uint32_t *)buffer[5].mapped)[0];
        memcpy(expected,buffer[2].mapped,(size_t)buffer[2].bytes);
        memset(buffer[2].mapped,0xcd,(size_t)buffer[2].bytes);memset(buffer[3].mapped,0,(size_t)buffer[3].bytes);
        memset(buffer[5].mapped,0,16u);
        ASSERT_TRUE(gather_submit(g,optimized));
        bool exact=memcmp(expected,buffer[2].mapped,(size_t)buffer[2].bytes)==0;
        printf("variant%u: exact particle state%d, decision reads%u -> %u\n",variant,exact,reference_reads,((uint32_t *)buffer[5].mapped)[0]);
        ASSERT_TRUE(exact);ASSERT_TRUE(((uint32_t *)buffer[5].mapped)[0]*3u<reference_reads);
        void *initial=malloc((size_t)buffer[0].bytes),*committed=malloc((size_t)buffer[0].bytes);
        void *cells=malloc((size_t)buffer[6].bytes),*counts=malloc((size_t)buffer[7].bytes);
        ASSERT_TRUE(initial&&committed&&cells&&counts);memcpy(initial,buffer[0].mapped,(size_t)buffer[0].bytes);
        ASSERT_TRUE(commit_submit(g,scalar_commit));
        memcpy(committed,buffer[0].mapped,(size_t)buffer[0].bytes);
        memcpy(cells,buffer[6].mapped,(size_t)buffer[6].bytes);memcpy(counts,buffer[7].mapped,(size_t)buffer[7].bytes);
        memcpy(buffer[0].mapped,initial,(size_t)buffer[0].bytes);reset_commit_metadata(buffer);
        ASSERT_TRUE(commit_submit(g,packed_commit));
        bool exact_commit=memcmp(committed,buffer[0].mapped,(size_t)buffer[0].bytes)==0&&
            memcmp(cells,buffer[6].mapped,(size_t)buffer[6].bytes)==0&&memcmp(counts,buffer[7].mapped,(size_t)buffer[7].bytes)==0;
        printf("variant%u exact committed particles, materials, counts%d\n",variant,exact_commit);
        free(initial);free(committed);free(cells);free(counts);ASSERT_TRUE(exact_commit);
    }
    vkDestroyPipeline(g->device,scalar_commit,NULL);vkDestroyPipeline(g->device,packed_commit,NULL);
    free(expected);vkDestroyPipeline(g->device,reference,NULL);vkDestroyPipeline(g->device,optimized,NULL);
    for(unsigned i=0;i<8;i++) {
        vkUnmapMemory(g->device,buffer[i].memory);vkDestroyBuffer(g->device,buffer[i].buffer,NULL);vkFreeMemory(g->device,buffer[i].memory,NULL);
    }
    dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_stationary_and_moving_gathers_match_the_ordered_reference_exactly);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
