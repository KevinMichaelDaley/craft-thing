#define DC_PROBE_WORDS 8192
#include "body_probe_fixture.h"

_Static_assert(PROBE_WORDS>=2048u+256u*2u,"Address probe output exceeds its fixture buffer");

static void test_full_width_entry_and_request_offsets_cross_32bit_boundaries(void) {
    probe_t p={0};ASSERT_TRUE(initialize(&p)&&allocate_buffer(&p));
    ASSERT_TRUE(create_pipeline(&p,"build/diagnostics/reactive_address_probe.spv"));
    enum {CASES=256};uint64_t expected[CASES];
    uint32_t indices[8]={0,1,31,UINT32_MAX,0x40000000u,1u<<24,1u<<29,100000000u};
    memset(p.mapped,0,PROBE_WORDS*4u);p.mapped[0]=CASES;
    for(uint32_t i=0;i<CASES;i++) {
        uint64_t base=UINT64_C(0xffffffa0)^((uint64_t)i<<32);
        uint32_t index=indices[(i/2u)%8u],*record=p.mapped+16u+i*4u;
        record[0]=(uint32_t)base;record[1]=(uint32_t)(base>>32);record[2]=index;record[3]=i%2u;
        expected[i]=base+(uint64_t)index*(i%2u?168u:144u);
    }
    ASSERT_TRUE(begin(&p));
    VkMemoryBarrier memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
        .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(p.command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memory,0,NULL,0,NULL);
    vkCmdBindPipeline(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    vkCmdBindDescriptorSets(p.command,VK_PIPELINE_BIND_POINT_COMPUTE,p.layout,0,1,&p.descriptor,0,NULL);
    vkCmdDispatch(p.command,CASES/64u,1,1);host_barrier(&p,1);
    ASSERT_TRUE(check(vkEndCommandBuffer(p.command),"reactive address command end")&&submit(&p,"reactive full-width addresses",1));
    uint32_t errors=0;
    for(uint32_t i=0;i<CASES;i++) {
        uint64_t actual=(uint64_t)p.mapped[2048u+i*2u]|((uint64_t)p.mapped[2049u+i*2u]<<32);
        errors+=actual!=expected[i];
    }
    printf("full-width entry/request cases%u address errors%u\n",CASES,errors);
    destroy_probe(&p);ASSERT_EQ(errors,0u);PASS();
}
int main(void) {
    RUN(test_full_width_entry_and_request_offsets_cross_32bit_boundaries);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
