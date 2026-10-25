#define main bounded_probe_main
#define DC_PROBE_BUFFER_DEVICE_ADDRESS
#include "../../tools/diagnostics/vulkan_probe.c"
#undef main
#include "../../tools/diagnostics/shader_fixture.h"
#include <math.h>

#ifndef TRANSPORT_TEST_WIDTH
#define TRANSPORT_TEST_WIDTH 137
#endif
#ifndef TRANSPORT_TEST_HEIGHT
#define TRANSPORT_TEST_HEIGHT 83
#endif
#ifndef TRANSPORT_TEST_STEP
#define TRANSPORT_TEST_STEP 6.f
#endif
enum { WIDTH=TRANSPORT_TEST_WIDTH,HEIGHT=TRANSPORT_TEST_HEIGHT,CELLS=WIDTH*HEIGHT,
    CHUNKS_X=(WIDTH+63)/64,CHUNKS=CHUNKS_X*((HEIGHT+63)/64),FULL=65536,TREE_BASE=16448 };
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();if(g_fail)return 1;} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
typedef struct {
    probe_t p;dc_gpu_t gpu;
    VkBuffer buffers[8];VkDeviceMemory memory[8];uint32_t *words[8];
    uint32_t expected[CELLS];
} fixture_t;
static const uint32_t bindings[]={1,2,3,8,10,28,29,32};
static const uint32_t sizes[]={CHUNKS*4096*2,CHUNKS,5*CELLS,CELLS,2*CELLS,2*CELLS,TREE_BASE+16384,CHUNKS*4096*4};
static uint32_t minimum(uint32_t a,uint32_t b) {return a<b?a:b;}
static uint32_t bits(float x) {uint32_t u;memcpy(&u,&x,4);return u;}
static float number(uint32_t x) {float f;memcpy(&f,&x,4);return f;}
static unsigned index_of(int x,int y) {return (unsigned)y*WIDTH+(unsigned)x;}
static uint32_t atlas(const fixture_t *f,int x,int y) {
    if(x<0||y<0||x>=WIDTH||y>=HEIGHT) return UINT32_MAX;
    uint32_t page=f->words[1][(y/64)*CHUNKS_X+x/64];
    return page?(page-1)*4096+(unsigned)(y%64)*64+(unsigned)(x%64):UINT32_MAX;
}
static bool permeable(uint32_t m) {return m==0||m==2||m==4||m==5||m==6;}
static uint32_t capacity(const fixture_t *f,int x,int y) {
    return FULL-minimum(FULL,f->words[2][2*CELLS+index_of(x,y)]);
}
static bool open_cell(const fixture_t *f,int x,int y) {
    uint32_t a=atlas(f,x,y);
    return a!=UINT32_MAX&&permeable(f->words[0][2*a]&65535)&&capacity(f,x,y)>0;
}
static uint32_t aperture(const fixture_t *f,int x,int y,bool vertical) {
    if(atlas(f,x,y)==UINT32_MAX) return 0;
    uint32_t value=FULL-minimum(FULL,f->words[2][(vertical?4:3)*CELLS+index_of(x,y)]);
    uint32_t a=atlas(f,x,y),b=atlas(f,x+!vertical,y+vertical);
    bool soil=(a!=UINT32_MAX&&(f->words[0][2*a]&65535u)==4u)||
        (b!=UINT32_MAX&&(f->words[0][2*b]&65535u)==4u);
    return soil?value/64u:value;
}

/* Literal old ordered GPU recurrence, used only as a diagnostic oracle. */
static void reference_move(fixture_t *f,int x,int y,int dx,int dy) {
    uint32_t a=atlas(f,x,y);if(a==UINT32_MAX) return;
    uint32_t own=index_of(x,y),available=f->expected[own];
    if(!available||!permeable(f->words[0][2*a]&65535)||!capacity(f,x,y)||!open_cell(f,x+dx,y+dy)) return;
    int fx=x+(dx<0?-1:0),fy=y+(dy<0?-1:0);bool vertical=dy!=0;
    float face=(float)aperture(f,fx,fy,vertical)/FULL;if(face<=0) return;
    unsigned fi=index_of(fx,fy);
    float speed=number(f->words[4][2*fi+(vertical?1:0)]);
    if(vertical) speed=.5f*(speed+number(f->words[3][fi]));
    if(dx<0||dy<0) speed=-speed;
    if(speed<=0) return;
    float travel=speed*(float)TRANSPORT_TEST_STEP*1.001f;
    uint32_t edge=vertical?(dy>0?HEIGHT-1-(unsigned)y:(unsigned)y):(dx>0?WIDTH-1-(unsigned)x:(unsigned)x);
    uint32_t reach=minimum((uint32_t)ceilf(travel),edge);
    if(reach==1) {
        unsigned destination=index_of(x+dx,y+dy);uint32_t cap=capacity(f,x+dx,y+dy),water=f->expected[destination];
        uint32_t allowance=(uint32_t)floorf(fminf(fmaxf(travel,0),1)*(float)FULL*face);
        uint32_t flux=minimum(minimum(allowance,available),water<cap?cap-water:0);
        f->expected[destination]+=flux;f->expected[own]-=flux;return;
    }
    for(unsigned offset=2;offset<=reach;offset++)
        if(!open_cell(f,x+dx*(int)offset,y+dy*(int)offset)||
           !aperture(f,fx+dx*(int)(offset-1),fy+dy*(int)(offset-1),vertical)) {reach=offset-1;break;}
    for(unsigned offset=reach;offset&&available;offset--) {
        int qx=x+dx*(int)offset,qy=y+dy*(int)offset;unsigned qi=index_of(qx,qy);
        uint32_t cap=capacity(f,qx,qy),water=f->expected[qi];if(water>=cap) continue;
        float fraction=fminf(fmaxf(travel-(float)(offset-1),0),1),path=face;
        for(unsigned e=1;e<offset;e++) path=fminf(path,(float)aperture(f,fx+dx*(int)e,fy+dy*(int)e,vertical)/FULL);
        uint32_t allowance=(uint32_t)floorf(fraction*(float)FULL*path);
        uint32_t flux=minimum(minimum(allowance,available),cap-water);
        f->expected[qi]+=flux;available-=flux;
    }
    f->expected[own]=available;
}

static void run_case_scaled(unsigned mode,unsigned unknown,float velocity_scale) {
    fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f);ASSERT_TRUE(initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
    for(unsigned i=0;i<8;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
        memset(f->words[i],0,sizes[i]*4);
    }
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=unknown&&i==1?0:i+1;
    bool vertical=mode<2,positive=(mode&1)==0;
    unsigned length=vertical?HEIGHT:WIDTH,lines=vertical?WIDTH:HEIGHT,leaves=1;
    while(leaves<(length+15)/16) leaves*=2;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        uint32_t a=atlas(f,(int)x,(int)y),i=index_of((int)x,(int)y),hash=x*7919+y*17239;
        uint32_t material=x==0||x+1==WIDTH||y+1==HEIGHT||(x==35&&y%9<2)?1:hash%17==0?4:0;
        uint32_t water=material==1?0:hash%5==0?FULL:hash%(FULL+1);
        if(a!=UINT32_MAX) {f->words[0][2*a]=material;f->words[0][2*a+1]=water;f->expected[i]=water;}
        if(x>=48&&x<80&&y>=18&&y<60) {
            f->words[2][2*CELLS+i]=(hash%3)*FULL/4;
            f->words[2][3*CELLS+i]=(hash%5)*FULL/4;
            f->words[2][4*CELLS+i]=((hash/7)%5)*FULL/4;
        }
        float velocity=(float)((x+y)%12+1)*(positive?1.f:-1.f)*velocity_scale;
        f->words[4][2*i+(vertical?1:0)]=bits(velocity);f->words[3][i]=bits(velocity*.5f);
    }
    /* Seed the already independently GPU-tested cache, isolating transport. */
    for(unsigned line=0;line<lines;line++) {
        unsigned base=TREE_BASE+line*2*leaves;
        for(unsigned tile=0;tile<leaves;tile++) {
            uint32_t value=FULL;
            for(unsigned k=0;k<16;k++) {
                unsigned p=tile*16+k;if(p>=length) continue;
                value=minimum(value,aperture(f,vertical?(int)line:(int)p,vertical?(int)p:(int)line,vertical));
                unsigned index=vertical?p*WIDTH+line:line*WIDTH+p;f->words[5][2*index]=value;
            }
            f->words[6][base+leaves+tile]=value;value=FULL;
            for(int k=15;k>=0;k--) {
                unsigned p=tile*16+(unsigned)k;if(p>=length) continue;
                value=minimum(value,aperture(f,vertical?(int)line:(int)p,vertical?(int)p:(int)line,vertical));
                unsigned index=vertical?p*WIDTH+line:line*WIDTH+p;f->words[5][2*index+1]=value;
            }
        }
        for(unsigned node=leaves-1;node;node--) f->words[6][base+node]=minimum(f->words[6][base+2*node],f->words[6][base+2*node+1]);
        for(int p=positive?(int)length-2:1;p>=0&&p<(int)length;p+=positive?-1:1)
            reference_move(f,vertical?(int)line:p,vertical?p:(int)line,vertical?0:positive?1:-1,vertical?positive?1:-1:0);
    }
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,"build/shaders/fluid_transport.comp.spv",bindings,8,mode));
    VkDescriptorBufferInfo info[8];VkWriteDescriptorSet writes[8];
    for(unsigned i=0;i<8;i++) {
        info[i]=(VkDescriptorBufferInfo){f->buffers[i],0,sizes[i]*4};
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
            .dstBinding=bindings[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&info[i]};
    }
    vkUpdateDescriptorSets(f->p.device,8,writes,0,NULL);ASSERT_TRUE(begin(&f->p));
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,mode,0,leaves,bits((float)TRANSPORT_TEST_STEP),1};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,lines,1,1);host_barrier(&f->p,1);ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);
    ASSERT_TRUE(submit(&f->p,"cooperative exact transport",1));
    unsigned mismatches=0;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        uint32_t a=atlas(f,(int)x,(int)y);if(a==UINT32_MAX) continue;
        if(f->words[0][2*a+1]!=f->expected[y*WIDTH+x]) {
            if(!mismatches) printf("first mismatch(%u,%u) %u expected%u\n",x,y,f->words[0][2*a+1],f->expected[y*WIDTH+x]);
            mismatches++;
        }
    }
    printf("mode%u unknown%u: %u exact transport mismatches\n",mode,unknown,mismatches);
    fixture_release_pipeline(&f->p);
    for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_EQ(mismatches,0u);PASS();
}
static void run_case(unsigned mode,unsigned unknown) {run_case_scaled(mode,unknown,1.f);}
static void test_downward_priority_and_fractional_apertures(void) {run_case(0,0);}
static void test_upward_priority_and_velocity_trapezoid(void) {run_case(1,0);}
static void test_rightward_priority_across_partial_tiles(void) {run_case(2,0);}
static void test_leftward_priority_across_partial_tiles(void) {run_case(3,0);}
static void test_unknown_chunks_block_vertical_flow(void) {run_case(1,1);}
static void test_unknown_chunks_block_horizontal_flow(void) {run_case(3,1);}
static void test_downward_one_unit_quantization_is_exact(void) {run_case_scaled(0,0,2e-6f);}
static void test_upward_one_unit_quantization_is_exact(void) {run_case_scaled(1,0,2e-6f);}
static void test_rightward_one_unit_quantization_is_exact(void) {run_case_scaled(2,0,2e-6f);}
static void test_leftward_one_unit_quantization_is_exact(void) {run_case_scaled(3,0,2e-6f);}
#ifdef FLUID_TRANSPORT_FIXTURE_ONLY
int transport_fixture_main(void) {
#else
int main(void) {
#endif
    RUN(test_downward_priority_and_fractional_apertures);RUN(test_upward_priority_and_velocity_trapezoid);
    RUN(test_rightward_priority_across_partial_tiles);RUN(test_leftward_priority_across_partial_tiles);
    RUN(test_unknown_chunks_block_vertical_flow);RUN(test_unknown_chunks_block_horizontal_flow);
    RUN(test_downward_one_unit_quantization_is_exact);RUN(test_upward_one_unit_quantization_is_exact);
    RUN(test_rightward_one_unit_quantization_is_exact);RUN(test_leftward_one_unit_quantization_is_exact);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
