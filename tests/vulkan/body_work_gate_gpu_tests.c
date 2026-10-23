#define DC_PROBE_PUSH_CONSTANT_BYTES 32
#include "body_probe_fixture.h"

static bool check_gate(probe_t *probe,dc_gpu_address_buffer_t *templates,
    dc_gpu_address_buffer_t *commands,dc_gpu_address_buffer_t *queue,
    uint32_t count,uint32_t committed,bool null_queue) {
    enum {COMMAND_COUNT=137};
    dc_body_allocation_batch_t *batch=queue->mapped;
    *batch=(dc_body_allocation_batch_t){.count=count,.committed=committed};
    memset(commands->mapped,0xab,(size_t)commands->bytes);
    dc_body_address_t addresses[]={address_words(templates->address),address_words(commands->address),
        address_words(null_queue?0:queue->address)};
    uint32_t constants[8]={0};memcpy(constants,addresses,sizeof(addresses));constants[6]=COMMAND_COUNT;
    if(!begin(probe)) return false;
    VkMemoryBarrier upload={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};
    vkCmdPipelineBarrier(probe->command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&upload,0,NULL,0,NULL);
    vkCmdBindPipeline(probe->command,VK_PIPELINE_BIND_POINT_COMPUTE,probe->pipeline);
    vkCmdPushConstants(probe->command,probe->layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(constants),constants);
    vkCmdDispatch(probe->command,2,2,1);host_barrier(probe,1);
    if(!check(vkEndCommandBuffer(probe->command),"gate command end")) return false;
    if(!submit(probe,"request-dependent work gate",1)) exit(2);
    bool active=!null_queue&&count>0u&&committed==0u;
    VkDispatchIndirectCommand *input=templates->mapped,*output=commands->mapped;
    uint32_t mismatches=0;
    for(uint32_t i=0;i<COMMAND_COUNT;i++) {
        VkDispatchIndirectCommand expected=active?input[i]:(VkDispatchIndirectCommand){0};
        mismatches+=memcmp(&output[i],&expected,sizeof(expected))!=0;
        mismatches+=input[i].x!=i+1u||input[i].y!=i%3u+1u||input[i].z!=1u;
    }
    uint32_t *guard=(uint32_t *)(output+COMMAND_COUNT);
    for(uint32_t i=0;i<4;i++) mismatches+=guard[i]!=0xababababu;
    printf("gate count %u committed %u null %u: %u mismatches\n",count,committed,null_queue,mismatches);
    return mismatches==0u;
}
static void test_empty_and_completed_queues_clear_every_stale_indirect_triplet(void) {
    probe_t probe={0};dc_gpu_address_buffer_t templates={0},commands={0},queue={0};
    dc_gpu_t gpu={0};char err[128];bool ok=true;
#define CHECK(e) do {if(!(e)){printf("gate failure %d: %s\n",__LINE__,#e);ok=false;goto done;}} while(0)
    CHECK(initialize(&probe));CHECK(allocate_buffer(&probe));
    gpu.device=probe.device;gpu.physical=probe.physical;
    CHECK(create_pipeline(&probe,"build/diagnostics/body_work_gate.spv"));
    CHECK(dc_gpu_address_buffer_create(&gpu,137u*sizeof(VkDispatchIndirectCommand),true,&templates,err,sizeof(err)));
    CHECK(dc_gpu_address_buffer_create(&gpu,137u*sizeof(VkDispatchIndirectCommand)+16u,true,&commands,err,sizeof(err)));
    CHECK(dc_gpu_address_buffer_create(&gpu,sizeof(dc_body_allocation_batch_t),true,&queue,err,sizeof(err)));
    VkDispatchIndirectCommand *input=templates.mapped;
    for(uint32_t i=0;i<137;i++) input[i]=(VkDispatchIndirectCommand){i+1u,i%3u+1u,1};
    CHECK(check_gate(&probe,&templates,&commands,&queue,90,0,false));
    CHECK(check_gate(&probe,&templates,&commands,&queue,0,0,false));
    CHECK(check_gate(&probe,&templates,&commands,&queue,90,90,false));
    CHECK(check_gate(&probe,&templates,&commands,&queue,90,0,true));
    CHECK(check_gate(&probe,&templates,&commands,&queue,90,0,false));
done:
    dc_gpu_address_buffer_destroy(&gpu,&queue);dc_gpu_address_buffer_destroy(&gpu,&commands);
    dc_gpu_address_buffer_destroy(&gpu,&templates);destroy_probe(&probe);
    if(ok) PASS();else g_fail++;
#undef CHECK
}
int main(void) {RUN(test_empty_and_completed_queues_clear_every_stale_indirect_triplet);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
