#define main copy_device_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
typedef struct {VkBuffer buffer;VkDeviceMemory memory;uint32_t *mapped;VkDeviceSize bytes;} copy_buffer_t;

static void copy_case(unsigned width,unsigned height) {
    probe_t p={0};ASSERT_TRUE(initialize(&p));char err[256];
    dc_gpu_t gpu={.device=p.device,.physical=p.physical,.width=width,.height=height};
    unsigned columns=(width+63)/64,rows=(height+63)/64,slots=columns*rows;
    unsigned tiles_x=(width+15)/16,tiles_y=(height+15)/16,tiles=tiles_x*tiles_y;
    copy_buffer_t buffer[4]={0};
    uint32_t bindings[]={2,19,22,27};
    for(unsigned i=0;i<4;i++) {
        buffer[i].bytes=i==0?slots*4u:i==3?(11u+2u*tiles)*4u:(VkDeviceSize)slots*8192u*56u;
        if(i==3&&buffer[i].bytes<156u) buffer[i].bytes=156u;
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&gpu,buffer[i].bytes,&buffer[i].buffer,&buffer[i].memory,
            (void **)&buffer[i].mapped,err,sizeof(err)));
        memset(buffer[i].mapped,0,(size_t)buffer[i].bytes);
    }
    for(unsigned slot=0;slot<slots;slot++) buffer[0].mapped[slot]=(slot*17u)%slots+1u;
    if(slots>1) buffer[0].mapped[slots/2]=0u;
    for(size_t word=0;word<buffer[1].bytes/4u;word++) {
        buffer[1].mapped[word]=0xbeef0000u^((uint32_t)word*2246822519u);
        buffer[2].mapped[word]=0x01000000u^((uint32_t)word*2654435761u);
    }
    uint32_t *expected=malloc((size_t)buffer[1].bytes);ASSERT_TRUE(expected);
    memcpy(expected,buffer[1].mapped,(size_t)buffer[1].bytes);
    unsigned active=0;
    for(unsigned tile=0;tile<tiles;tile++) {
        if(tile%3u==1u) continue;
        buffer[3].mapped[3u+active++]=tile;
        for(unsigned dy=0;dy<16;dy++) for(unsigned dx=0;dx<16;dx++) {
            unsigned x=(tile%tiles_x)*16+dx,y=(tile/tiles_x)*16+dy;
            if(x>=width||y>=height) continue;
            unsigned page=buffer[0].mapped[(y/64)*columns+x/64];if(!page) continue;
            for(unsigned layer=0;layer<2;layer++) {
                size_t particle=(page-1u)*8192u+layer*4096u+(y%64)*64u+x%64;
                memcpy(expected+particle*14u,buffer[2].mapped+particle*14u,56u);
            }
        }
    }
    buffer[3].mapped[10u+2u*tiles]=active;
    p.buffer=buffer[1].buffer;p.mapped=buffer[1].mapped;
    ASSERT_TRUE(fixture_body_pipeline(&p,&gpu,VK_NULL_HANDLE,"build/shaders/mpm_copy.comp.spv",bindings,4,UINT32_MAX));
    VkDescriptorBufferInfo info[4];VkWriteDescriptorSet writes[4];
    for(unsigned i=0;i<4;i++) {
        info[i]=(VkDescriptorBufferInfo){buffer[i].buffer,0,buffer[i].bytes};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=p.descriptor,
            .dstBinding=bindings[i],.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=info+i};
    }
    vkUpdateDescriptorSets(p.device,4,writes,0,NULL);
    ASSERT_TRUE(begin(&p));
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    uint32_t push[8]={width,height};
    vkCmdPushConstants(p.command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(p.command,active,1,1);host_barrier(&p,1);
    ASSERT_EQ(vkEndCommandBuffer(p.command),VK_SUCCESS);ASSERT_TRUE(submit(&p,"packed particle copy",1));
    bool exact=memcmp(expected,buffer[1].mapped,(size_t)buffer[1].bytes)==0;
    printf("%ux%u: exact active/inactive particle bytes%d\n",width,height,exact);
    free(expected);fixture_release_pipeline(&p);
    for(unsigned i=0;i<4;i++) {
        vkUnmapMemory(p.device,buffer[i].memory);vkDestroyBuffer(p.device,buffer[i].buffer,NULL);vkFreeMemory(p.device,buffer[i].memory,NULL);
    }
    vkDestroyFence(p.device,p.fence,NULL);vkDestroyCommandPool(p.device,p.pool,NULL);
    vkDestroyDevice(p.device,NULL);vkDestroyInstance(p.instance,NULL);
    ASSERT_TRUE(exact);
}
static void test_packed_copy_preserves_inactive_chunks_and_partial_cells(void) {
    const unsigned dimensions[][2]={{64,64},{137,83},{8,8},{640,360},{128,128}};
    for(unsigned i=0;i<5;i++) {copy_case(dimensions[i][0],dimensions[i][1]);if(g_fail)return;}
    PASS();
}
int main(void) {
    RUN(test_packed_copy_preserves_inactive_chunks_and_partial_cells);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
