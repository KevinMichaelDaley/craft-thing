#define FLUID_TRANSPORT_FIXTURE_ONLY
#include "fluid_transport_tests.c"
#ifdef DC_DISPLACEMENT_PROFILE
static bool g_no_body;
#endif

/* Literal pre-optimization displacement order; never used by runtime physics. */
static void reference_displacement(fixture_t *f,bool lateral) {
    for(unsigned line=0;line<(lateral?HEIGHT:WIDTH);line++)
        for(int position=lateral?0:HEIGHT-1;position>=0&&position<(lateral?WIDTH:HEIGHT);position+=lateral?1:-1) {
            int x=lateral?position:(int)line,y=lateral?(int)line:position;
            uint32_t source=atlas(f,x,y),index=index_of(x,y);
            if(source==UINT32_MAX||f->words[2][index]==0) continue;
            uint32_t retained=minimum(f->expected[index],capacity(f,x,y));
            uint32_t remaining=f->expected[index]-retained;
            if(lateral) {
                bool left=true,right=true;
                for(int distance=1;distance<=64&&remaining;distance++) {
                    for(int side=-1;side<=1;side+=2) {
                        if((side<0&&!left)||(side>0&&!right)) continue;
                        int qx=x+side*distance;uint32_t target=atlas(f,qx,y);
                        if(target==UINT32_MAX||!permeable(f->words[0][2*target]&65535u)) {
                            if(side<0) left=false;else right=false;continue;
                        }
                        unsigned qi=index_of(qx,y);uint32_t water=f->expected[qi],cap=capacity(f,qx,y);
                        uint32_t flux=minimum(remaining,water<cap?cap-water:0);
                        f->expected[qi]+=flux;remaining-=flux;
                    }
                    if(!left&&!right) break;
                }
            } else for(int above=y-1;above>=0&&remaining;above--) {
                uint32_t target=atlas(f,x,above);
                if(target==UINT32_MAX||!permeable(f->words[0][2*target]&65535u)) break;
                unsigned qi=index_of(x,above);uint32_t water=f->expected[qi],cap=capacity(f,x,above);
                uint32_t flux=minimum(remaining,water<cap?cap-water:0);
                f->expected[qi]+=flux;remaining-=flux;
            }
            f->expected[index]=retained+remaining;
        }
}
static void displacement_case(bool lateral,unsigned unknown) {
    fixture_t *f=calloc(1,sizeof(*f));ASSERT_TRUE(f);ASSERT_TRUE(initialize(&f->p));
    f->gpu=(dc_gpu_t){.device=f->p.device,.physical=f->p.physical};char err[256];
    for(unsigned i=0;i<8;i++) {
        ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,sizes[i]*4,&f->buffers[i],&f->memory[i],(void **)&f->words[i],err,sizeof(err)));
        memset(f->words[i],0,sizes[i]*4);
    }
    for(unsigned i=0;i<CHUNKS;i++) f->words[1][i]=unknown&&i==1?0:i+1;
    unsigned x0=WIDTH>2048?2020:WIDTH<16?0:48,y0=HEIGHT>2048?2020:HEIGHT<16?0:18;
    unsigned x1=minimum(WIDTH-1,x0+64),y1=minimum(HEIGHT-1,y0+64);
    uint64_t initial_mass=0;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        uint32_t a=atlas(f,(int)x,(int)y),index=index_of((int)x,(int)y),hash=x*7919+y*17239;
        bool body=x>=x0&&x<x1&&y>=y0&&y<y1;
#ifdef DC_DISPLACEMENT_PROFILE
        if(g_no_body) body=false;
#endif
        uint32_t material=x==0||x+1==WIDTH||y+1==HEIGHT?1:hash%17==0?4:0;
        if(body&&WIDTH<2048&&HEIGHT<2048&&hash%29==0) material=1;
        uint32_t water=body?FULL:material==1?0:hash%(FULL+1);
        if(body) {f->words[2][index]=(x+y)%5?37:0;f->words[2][2*CELLS+index]=(hash%4+1)*FULL/4;}
        if(a!=UINT32_MAX) {f->words[0][2*a]=material;f->words[0][2*a+1]=water;f->expected[index]=water;initial_mass+=water;}
    }
    uint32_t expected_visits=0;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        unsigned index=index_of(x,y);
        if(atlas(f,(int)x,(int)y)!=UINT32_MAX&&f->words[2][index]!=0u&&
           f->expected[index]>capacity(f,x,y)) expected_visits++;
    }
    reference_displacement(f,lateral);
    f->p.buffer=f->buffers[5];f->p.mapped=f->words[5];
    const char *shader="build/shaders/fluid_displacement.comp.spv";
    uint32_t indices[9];memcpy(indices,bindings,sizeof(bindings));
    unsigned descriptors=8;
#ifdef DC_DISPLACEMENT_PROFILE
    VkBuffer profile_buffer;VkDeviceMemory profile_memory;uint32_t *profile;
    ASSERT_TRUE(dc_gpu_make_mapped_buffer(&f->gpu,4,&profile_buffer,&profile_memory,(void **)&profile,err,sizeof(err)));
    *profile=0;indices[8]=31;descriptors=9;
    shader="build/diagnostics/fluid_displacement_profile.comp.spv";
#endif
    ASSERT_TRUE(fixture_body_pipeline(&f->p,&f->gpu,VK_NULL_HANDLE,shader,indices,descriptors,lateral));
    VkDescriptorBufferInfo info[9];VkWriteDescriptorSet writes[9];
    for(unsigned i=0;i<descriptors;i++) {
        if(i<8) info[i]=(VkDescriptorBufferInfo){f->buffers[i],0,sizes[i]*4};
#ifdef DC_DISPLACEMENT_PROFILE
        else info[i]=(VkDescriptorBufferInfo){profile_buffer,0,4};
#endif
        writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=f->p.descriptor,
            .dstBinding=indices[i],.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&info[i]};
    }
    vkUpdateDescriptorSets(f->p.device,descriptors,writes,0,NULL);ASSERT_TRUE(begin(&f->p));
    vkCmdBindPipeline(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.pipeline);
    vkCmdBindDescriptorSets(f->p.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->p.layout,0,1,&f->p.descriptor,0,NULL);
    uint32_t push[8]={WIDTH,HEIGHT,lateral?12u:11u};
    vkCmdPushConstants(f->p.command,f->p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(f->p.command,lateral?HEIGHT:WIDTH,1,1);host_barrier(&f->p,1);
    ASSERT_EQ(vkEndCommandBuffer(f->p.command),VK_SUCCESS);ASSERT_TRUE(submit(&f->p,"cooperative exact displacement",1));
    unsigned mismatches=0;uint64_t mass=0;
    for(unsigned y=0;y<HEIGHT;y++) for(unsigned x=0;x<WIDTH;x++) {
        uint32_t a=atlas(f,(int)x,(int)y);if(a==UINT32_MAX) continue;
        uint32_t water=f->words[0][2*a+1];mass+=water;
        if(water!=f->expected[y*WIDTH+x]) {
            if(!mismatches) printf("first mismatch(%u,%u) %u expected%u\n",x,y,water,f->expected[y*WIDTH+x]);
            mismatches++;
        }
    }
    printf("%ux%u lateral%u unknown%u: %u exact displacement mismatches, mass%llu/%llu\n",
        WIDTH,HEIGHT,lateral,unknown,mismatches,(unsigned long long)mass,(unsigned long long)initial_mass);
#ifdef DC_DISPLACEMENT_PROFILE
    uint32_t visits=*profile;
    printf("source visits %u, initial excess-water candidates %u\n",visits,expected_visits);
    vkUnmapMemory(f->p.device,profile_memory);
    vkDestroyBuffer(f->p.device,profile_buffer,NULL);vkFreeMemory(f->p.device,profile_memory,NULL);
#else
    (void)expected_visits;
#endif
    fixture_release_pipeline(&f->p);
    for(unsigned i=0;i<8;i++) {vkUnmapMemory(f->p.device,f->memory[i]);vkDestroyBuffer(f->p.device,f->buffers[i],NULL);vkFreeMemory(f->p.device,f->memory[i],NULL);}
    vkDestroyFence(f->p.device,f->p.fence,NULL);vkDestroyCommandPool(f->p.device,f->p.pool,NULL);
    vkDestroyDevice(f->p.device,NULL);vkDestroyInstance(f->p.instance,NULL);free(f);
    ASSERT_EQ(mismatches,0u);ASSERT_EQ(mass,initial_mass);
#ifdef DC_DISPLACEMENT_PROFILE
    ASSERT_EQ(visits,expected_visits);
#endif
    PASS();
}
static void test_column_priority_and_unresolved_mass_are_exact(void) {displacement_case(false,0);}
static void test_left_then_right_priority_is_exact(void) {displacement_case(true,0);}
static void test_unknown_columns_retain_unresolved_water(void) {displacement_case(false,1);}
static void test_unknown_rows_close_only_the_blocked_side(void) {displacement_case(true,1);}
int main(void) {
    RUN(test_column_priority_and_unresolved_mass_are_exact);RUN(test_left_then_right_priority_is_exact);
    RUN(test_unknown_columns_retain_unresolved_water);RUN(test_unknown_rows_close_only_the_blocked_side);
#ifdef DC_DISPLACEMENT_PROFILE
    g_no_body=true;
    RUN(test_column_priority_and_unresolved_mass_are_exact);
    RUN(test_left_then_right_priority_is_exact);
#endif
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
