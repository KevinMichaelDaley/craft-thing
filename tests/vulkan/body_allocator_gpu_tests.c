#define DC_PROBE_PUSH_CONSTANT_BYTES 24
#include "body_probe_fixture.h"
#include "../../src/vulkan/body_scan_gpu.h"
#include "../../src/vulkan/body_storage_gpu.h"
typedef struct {
    probe_t probe;
    dc_gpu_t gpu;
    dc_gpu_body_store_t store;
    dc_gpu_body_scan_plan_t plan;
    dc_gpu_address_buffer_t scratch,queue,readback;
    VkPipeline reserve,commit,scan,add;
    VkShaderModule scan_module;
    uint64_t submission;
} allocator_fixture_t;
static VkPipeline specialized(allocator_fixture_t *f,VkShaderModule module,uint32_t phase) {
    VkSpecializationMapEntry entry={.constantID=0,.size=sizeof(phase)};
    VkSpecializationInfo specialization={.mapEntryCount=1,.pMapEntries=&entry,.dataSize=sizeof(phase),.pData=&phase};
    VkComputePipelineCreateInfo info={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=f->probe.layout,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,
            .module=module,.pName="main",.pSpecializationInfo=&specialization}};
    VkPipeline pipeline=VK_NULL_HANDLE;
    if(!check(vkCreateComputePipelines(f->gpu.device,VK_NULL_HANDLE,1,&info,NULL,&pipeline),"allocator pipeline")) return VK_NULL_HANDLE;
    return pipeline;
}
static void memory_barrier(allocator_fixture_t *f,VkPipelineStageFlags source,VkAccessFlags access,
    VkPipelineStageFlags destination,VkAccessFlags destination_access) {
    VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=access,.dstAccessMask=destination_access};
    vkCmdPipelineBarrier(f->probe.command,source,destination,0,1,&barrier,0,NULL,0,NULL);
}
static bool finish_submission(allocator_fixture_t *f,const char *label) {
    if(!check(vkEndCommandBuffer(f->probe.command),"allocator command end")) return false;
    if(!submit(&f->probe,label,1)) exit(2);
    return true;
}
static bool grow_store(allocator_fixture_t *f,uint32_t slots) {
    char err[128];dc_gpu_body_growth_t growth={0};
    if(!dc_gpu_body_growth_prepare(&f->gpu,&f->store,slots,&growth,err,sizeof(err))) {puts(err);return false;}
    dc_gpu_body_scan_plan_destroy(&f->plan);
    dc_gpu_address_buffer_destroy(&f->gpu,&f->scratch);dc_gpu_address_buffer_destroy(&f->gpu,&f->readback);
    if(!dc_gpu_body_scan_plan_create(growth.slab_count,65535,65535,&f->plan,err,sizeof(err))||
       !dc_gpu_address_buffer_create(&f->gpu,f->plan.bytes,false,&f->scratch,err,sizeof(err))||
       !dc_gpu_address_buffer_create(&f->gpu,growth.directory.bytes,true,&f->readback,err,sizeof(err))) {
        dc_gpu_body_growth_discard(&f->gpu,&growth);puts(err);return false;
    }
    if(!begin(&f->probe)) return false;
    if(!dc_gpu_body_growth_record(f->probe.command,f->probe.buffer,&f->store,&growth,++f->submission)) return false;
    dc_body_address_t pointers[]={address_words(f->scratch.address),address_words(f->queue.address)};
    vkCmdUpdateBuffer(f->probe.command,f->probe.buffer,offsetof(dc_body_storage_root_t,work),sizeof(pointers),pointers);
    memory_barrier(f,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    if(!finish_submission(f,"allocator store growth")) return false;
    return dc_gpu_body_growth_finish(&f->gpu,&f->store,&growth,f->submission);
}
static bool allocate_batch(allocator_fixture_t *f) {
    if(!begin(&f->probe)) return false;
    memory_barrier(f,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindDescriptorSets(f->probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->probe.layout,0,1,&f->probe.descriptor,0,NULL);
    vkCmdBindPipeline(f->probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->probe.pipeline);
    vkCmdDispatch(f->probe.command,f->store.slab_count,1,1);
    if(!dc_gpu_body_scan_record(f->probe.command,&f->plan,f->scratch.address,f->scratch.bytes,f->probe.layout,f->scan,f->add)) return false;
    vkCmdBindPipeline(f->probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->reserve);
    vkCmdDispatch(f->probe.command,f->store.slab_count,1,1);
    memory_barrier(f,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(f->probe.command,VK_PIPELINE_BIND_POINT_COMPUTE,f->commit);
    vkCmdDispatch(f->probe.command,f->store.slab_count,1,1);
    memory_barrier(f,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferCopy copy={.size=f->readback.bytes};
    vkCmdCopyBuffer(f->probe.command,f->store.directory.buffer,f->readback.buffer,1,&copy);
    memory_barrier(f,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_HOST_READ_BIT);
    return finish_submission(f,"parallel body allocation");
}
static void set_batch(allocator_fixture_t *f,uint32_t count,uint32_t commit) {
    memset(f->queue.mapped,0,(size_t)f->queue.bytes);
    dc_body_allocation_batch_t *batch=f->queue.mapped;
    *batch=(dc_body_allocation_batch_t){.count=count,.commit=commit,.transaction={17,29}};
    dc_body_address_t *handles=(dc_body_address_t *)(batch+1);
    for(uint32_t i=0;i<count;i++) handles[i]=(dc_body_address_t){DC_BODY_SLOT_NONE,0};
}
static void test_parallel_reservation_is_atomic_and_retries_through_growth(void) {
    allocator_fixture_t f={0};bool ok=true;char err[128];
#define CHECK(e) do {if(!(e)){printf("allocation failure line%d: %s\n",__LINE__,#e);ok=false;goto done;}} while(0)
    CHECK(initialize(&f.probe));CHECK(allocate_buffer(&f.probe));
    f.gpu.device=f.probe.device;f.gpu.physical=f.probe.physical;
    CHECK(create_pipeline(&f.probe,"build/diagnostics/body_allocator.spv"));
    f.reserve=specialized(&f,f.probe.shader,1);f.commit=specialized(&f,f.probe.shader,2);CHECK(f.reserve&&f.commit);
    CHECK(dc_gpu_load_shader_module(&f.gpu,"build/diagnostics/body_scan.spv",&f.scan_module,err,sizeof(err)));
    f.scan=specialized(&f,f.scan_module,0);f.add=specialized(&f,f.scan_module,1);CHECK(f.scan&&f.add);
    CHECK(dc_gpu_address_buffer_create(&f.gpu,sizeof(dc_body_allocation_batch_t)+90u*sizeof(dc_body_address_t),true,&f.queue,err,sizeof(err)));
    CHECK(grow_store(&f,90));
    CHECK(begin(&f.probe));
    memory_barrier(&f,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    uint32_t masks[][2]={{~(1u<<3),UINT32_MAX},{~(1u<<1),UINT32_MAX}};
    for(uint32_t page=0;page<2;page++) vkCmdUpdateBuffer(f.probe.command,f.store.directory.buffer,
        page*sizeof(dc_body_storage_page_t)+offsetof(dc_body_storage_page_t,free_mask),sizeof(masks[page]),masks[page]);
    CHECK(finish_submission(&f,"allocator retired slots"));
    set_batch(&f,90,0);CHECK(allocate_batch(&f));
    dc_body_allocation_batch_t *batch=f.queue.mapped;
    dc_body_address_t *handles=(dc_body_address_t *)(batch+1);
    dc_body_storage_root_t *root=(dc_body_storage_root_t *)f.probe.mapped;
    CHECK(batch->ready==1u&&batch->committed==0u&&root->live_count==0u);
    for(uint32_t i=0;i<90;i++) {uint32_t expected=i+(i>=3)+(i>=64);CHECK(handles[i].low==expected&&handles[i].high==1u);}
    dc_body_storage_page_t *pages=f.readback.mapped;
    CHECK(pages[0].free_mask[0]==masks[0][0]&&pages[1].free_mask[0]==masks[1][0]);
    batch->commit=1;CHECK(allocate_batch(&f));CHECK(batch->committed==90u&&root->live_count==90u);
    CHECK(allocate_batch(&f));CHECK(root->live_count==90u);
    set_batch(&f,40,1);CHECK(allocate_batch(&f));
    CHECK(!batch->ready&&!batch->committed&&batch->error==DC_BODY_ALLOCATION_NO_CAPACITY&&batch->required_slots==132u);
    CHECK(root->live_count==90u);CHECK(grow_store(&f,batch->required_slots));CHECK(allocate_batch(&f));
    CHECK(batch->ready&&batch->committed==40u&&root->live_count==130u);
    for(uint32_t i=0;i<40;i++) CHECK(handles[i].low==92u+i&&handles[i].high==1u);
done:
    dc_gpu_body_store_destroy(&f.gpu,&f.store);dc_gpu_body_scan_plan_destroy(&f.plan);
    dc_gpu_address_buffer_destroy(&f.gpu,&f.queue);dc_gpu_address_buffer_destroy(&f.gpu,&f.scratch);dc_gpu_address_buffer_destroy(&f.gpu,&f.readback);
    if(f.reserve) vkDestroyPipeline(f.gpu.device,f.reserve,NULL);
    if(f.commit) vkDestroyPipeline(f.gpu.device,f.commit,NULL);
    if(f.scan) vkDestroyPipeline(f.gpu.device,f.scan,NULL);
    if(f.add) vkDestroyPipeline(f.gpu.device,f.add,NULL);
    if(f.scan_module) vkDestroyShaderModule(f.gpu.device,f.scan_module,NULL);
    if(f.probe.device) destroy_probe(&f.probe);
    ASSERT_TRUE(ok);PASS();
#undef CHECK
}
int main(void) {RUN(test_parallel_reservation_is_atomic_and_retries_through_growth);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
