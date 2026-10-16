#define main bounded_probe_main
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"

enum { WIDTH = 137, HEIGHT = 83, CELLS = WIDTH * HEIGHT, FULL = 65536, TREE_BASE = 16448 };
static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %d: %s\n", __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
typedef struct {
    probe_t p;
    dc_gpu_t gpu;
    VkBuffer buffers[5];
    VkDeviceMemory memory[5];
    uint32_t *words[5];
} fixture_t;
static const uint32_t bindings[] = {2,3,28,29,30};
static const uint32_t sizes[] = {6,5*CELLS,2*CELLS,TREE_BASE+16384,4*CELLS};

static bool pipeline(fixture_t *f,const char *path) {
    if (!fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,path,bindings,5,UINT32_MAX)) return false;
    VkDescriptorBufferInfo buffers[5]; VkWriteDescriptorSet writes[5];
    for (unsigned i=0;i<5;i++) {
        buffers[i]=(VkDescriptorBufferInfo){f->buffers[i],0,sizes[i]*sizeof(uint32_t)};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet=f->p.descriptor,.dstBinding=bindings[i],.descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffers[i]};
    }
    vkUpdateDescriptorSets(f->p.device,5,writes,0,NULL);
    return true;
}
static void bind(fixture_t *f) {
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
}
static void barrier(fixture_t *f) {
    VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(f->p.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memory,0,NULL,0,NULL);
}
static uint32_t reference(const fixture_t *f,unsigned x,unsigned y,unsigned axis) {
    if (!f->words[0][(y/64)*3+x/64]) return 0;
    unsigned covered=f->words[1][(axis?4:3)*CELLS+y*WIDTH+x];
    return FULL-(covered<FULL?covered:FULL);
}
static void run_case(unsigned axis,unsigned unknown) {
    fixture_t *f=calloc(1,sizeof(*f)); ASSERT_TRUE(f);
    ASSERT_TRUE(initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};
    char err[256];
    for (unsigned i=0;i<5;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*sizeof(uint32_t),&f->buffers[i],
            &f->memory[i],(void **)&f->words[i],err,sizeof(err)));
        memset(f->words[i],0xa5,sizes[i]*sizeof(uint32_t));
    }
    for (unsigned i=0;i<6;i++) f->words[0][i]=unknown&&i==1?0:i+1;
    for (unsigned y=0;y<HEIGHT;y++) for (unsigned x=0;x<WIDTH;x++) {
        uint32_t hash=(x*12413u+y*17239u)^((x+y)*7919u);
        f->words[1][3*CELLS+y*WIDTH+x]=hash%11==0?FULL:hash%(FULL+1u);
        f->words[1][4*CELLS+y*WIDTH+x]=hash%13==0?0:(hash*7u)%(FULL+1u);
    }
    f->p.buffer=f->buffers[2]; f->p.mapped=f->words[2];
    ASSERT_TRUE(pipeline(f,"build/shaders/fluid_aperture.comp.spv"));
    unsigned length=axis?HEIGHT:WIDTH,lines=axis?WIDTH:HEIGHT,leaves=1;
    while (leaves<(length+15)/16) leaves*=2;
    ASSERT_TRUE(begin(&f->p));bind(f);
    uint32_t push[8]={WIDTH,HEIGHT,axis,0,leaves};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,lines*((leaves+3)/4),1,1);barrier(f);
    for (unsigned nodes=leaves/2;nodes;nodes/=2) {
        push[3]=nodes;
        vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(f->p.command,(lines*nodes+63)/64,1,1);barrier(f);
    }
    host_barrier(&f->p,1);ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&f->p,"aperture cache",1));
    unsigned mismatches=0;
    for (unsigned y=0;y<HEIGHT;y++) for (unsigned x=0;x<WIDTH;x++) {
        unsigned coordinate=axis?y:x,low=coordinate/16*16,high=low+16;
        if(high>length) high=length;
        uint32_t prefix=FULL,suffix=FULL;
        for(unsigned k=low;k<=coordinate;k++) {
            uint32_t value=reference(f,axis?x:k,axis?k:y,axis);
            if(value<prefix) prefix=value;
        }
        for(unsigned k=coordinate;k<high;k++) {
            uint32_t value=reference(f,axis?x:k,axis?k:y,axis);
            if(value<suffix) suffix=value;
        }
        unsigned i=y*WIDTH+x;
        mismatches+=f->words[2][2*i]!=prefix||f->words[2][2*i+1]!=suffix;
    }
    ASSERT_TRUE(pipeline(f,"build/shaders/fluid_aperture_query.comp.spv"));
    ASSERT_TRUE(begin(&f->p));bind(f);
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,(CELLS+63)/64,1,1);host_barrier(&f->p,1);
    ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&f->p,"aperture exact queries",1));
    for(unsigned q=0;q<4;q++) for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        unsigned coordinate=axis?y:x,available=q<2?length-1-coordinate:coordinate;
        unsigned distance=(q&1)?available:(available<7?available:7);
        unsigned start=q<2?coordinate:coordinate-distance;
        uint32_t expected=FULL;
        for(unsigned k=0;k<distance;k++) {
            uint32_t value=reference(f,axis?x:start+k,axis?start+k:y,axis);
            if(value<expected) expected=value;
        }
        float value=(float)expected/FULL;uint32_t bits;memcpy(&bits,&value,sizeof(bits));
        mismatches+=f->words[4][q*CELLS+y*WIDTH+x]!=bits;
    }
    printf("axis%u unknown%u: %u exact cache/query mismatches\n",axis,unknown,mismatches);
    fixture_release_pipeline(&f->p);
    for(unsigned i=0;i<5;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_EQ(mismatches,0u);PASS();
}
static void test_horizontal_fractional_ranges_are_exact(void) {run_case(0,0);}
static void test_vertical_partial_tiles_are_exact(void) {run_case(1,0);}
static void test_unknown_pages_are_conservative(void) {run_case(0,1);}
int main(void) {
    RUN(test_horizontal_fractional_ranges_are_exact);RUN(test_vertical_partial_tiles_are_exact);
    RUN(test_unknown_pages_are_conservative);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
