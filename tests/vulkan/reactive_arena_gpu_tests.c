#include "body_probe_fixture.h"
#include "../../src/vulkan/reactive_arena_gpu.h"
#include "dungeoncraft/reactive_state_layout.h"

static bool reject_arena_allocations;
VKAPI_ATTR VkResult VKAPI_CALL __real_vkAllocateMemory(VkDevice,const VkMemoryAllocateInfo *,const VkAllocationCallbacks *,VkDeviceMemory *);
VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo *i,const VkAllocationCallbacks *a,VkDeviceMemory *out) {
    return reject_arena_allocations?VK_ERROR_OUT_OF_DEVICE_MEMORY:__real_vkAllocateMemory(d,i,a,out);
}
typedef struct {probe_t probe;dc_gpu_t gpu;fr_reactive_arena_gpu_t arena;} fixture_t;
static bool fixture(fixture_t *f,char *err,uint32_t cap) {
    if(!initialize(&f->probe)||!allocate_buffer(&f->probe)) return false;
    f->gpu=(dc_gpu_t){.physical=f->probe.physical,.device=f->probe.device,
        .queue=f->probe.queue,.command=f->probe.command};
    return fr_reactive_arena_create(&f->gpu,64,&f->arena,err,cap);
}
static void cleanup(fixture_t *f) {
    fr_reactive_arena_destroy(&f->gpu,&f->arena);destroy_probe(&f->probe);
}
static bool records(fr_reactive_arena_request_t *requests,uint32_t count,uint32_t first) {
    char err[128];
    memset(requests,0,(size_t)count*sizeof(*requests));
    for(uint32_t i=0;i<count;i++) {
        requests[i].operation=FR_REACTIVE_ARENA_ALLOCATE;
        requests[i].record.owner=fr_reactive_cell_owner((dc_chunk_coord_t){-9,3},(first+i)%4096u);
        if(!fr_reactive_state_default(DC_MATERIAL_WOOD,39322,0,700,first+i+1u,
            &requests[i].record.state,err,sizeof(err))) return false;
        requests[i].record.state.ignition_threshold=.8f;requests[i].record.state.ignition_integral=.4f;
    }
    return true;
}
static void test_gpu_allocator_reuses_freed_slots_without_reviving_stale_handles(void) {
    char err[256]={0};fixture_t f={0};ASSERT_TRUE(fixture(&f,err,sizeof(err)));
    fr_reactive_arena_request_t request[64];fr_reactive_handle_t original[64],replacement[32];uint32_t needed=0;
    ASSERT_TRUE(records(request,64,0));
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,64,original,&needed,err,sizeof(err)));
    for(uint32_t i=0;i<32;i++) request[i]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_RETIRE,.handle=original[i]};
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,32,replacement,&needed,err,sizeof(err)));
    ASSERT_TRUE(records(request,32,1000));
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,32,replacement,&needed,err,sizeof(err)));
    fr_reactive_arena_stats_t stats;ASSERT_TRUE(fr_reactive_arena_stats(&f.gpu,&f.arena,&stats));
    ASSERT_EQ(stats.capacity,64u);ASSERT_EQ(stats.live,64u);ASSERT_EQ(stats.free,0u);
    for(uint32_t i=0;i<32;i++) {
        fr_reactive_record_t out;bool found=true;
        ASSERT_TRUE(fr_reactive_arena_read(&f.gpu,&f.arena,original[i],&out,&found,err,sizeof(err)));ASSERT_TRUE(!found);
        ASSERT_EQ(replacement[i].generation,2u);
        ASSERT_TRUE(fr_reactive_arena_read(&f.gpu,&f.arena,replacement[i],&out,&found,err,sizeof(err)));ASSERT_TRUE(found);
        ASSERT_TRUE(memcmp(&out,&request[i].record,sizeof(out))==0);
    }
    cleanup(&f);PASS();
}
static void test_failed_growth_rolls_back_the_whole_gpu_batch_and_then_retries(void) {
    char err[256]={0};fixture_t f={0};ASSERT_TRUE(fixture(&f,err,sizeof(err)));
    fr_reactive_arena_request_t request[64];fr_reactive_handle_t original[64],out[5],untouched[5];uint32_t needed=0;
    ASSERT_TRUE(records(request,64,0));
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,64,original,&needed,err,sizeof(err)));
    uint32_t snapshot[64*FR_ENTRY_WORDS];memcpy(snapshot,f.arena.entries.mapped,sizeof(snapshot));
    ASSERT_TRUE(records(request,5,1000));memset(out,0xa5,sizeof(out));memcpy(untouched,out,sizeof(out));
    reject_arena_allocations=true;
    bool accepted=fr_reactive_arena_apply(&f.gpu,&f.arena,request,5,out,&needed,err,sizeof(err));
    reject_arena_allocations=false;
    ASSERT_TRUE(!accepted);ASSERT_TRUE(needed>=69u);ASSERT_TRUE(memcmp(out,untouched,sizeof(out))==0);
    ASSERT_TRUE(memcmp(snapshot,f.arena.entries.mapped,sizeof(snapshot))==0);
    fr_reactive_arena_stats_t stats;ASSERT_TRUE(fr_reactive_arena_stats(&f.gpu,&f.arena,&stats));
    ASSERT_EQ(stats.live,64u);ASSERT_EQ(stats.free,0u);ASSERT_EQ(stats.capacity,64u);
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,5,out,&needed,err,sizeof(err)));
    ASSERT_TRUE(fr_reactive_arena_stats(&f.gpu,&f.arena,&stats));ASSERT_TRUE(stats.capacity>=69u);ASSERT_EQ(stats.live,69u);
    for(uint32_t i=0;i<5;i++) {
        fr_reactive_record_t record;bool found=false;
        ASSERT_TRUE(fr_reactive_arena_read(&f.gpu,&f.arena,out[i],&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
        ASSERT_TRUE(memcmp(&record,&request[i].record,sizeof(record))==0);
    }
    cleanup(&f);PASS();
}
static void test_stale_or_duplicate_retire_requests_cannot_partially_erase_a_batch(void) {
    char err[256]={0};fixture_t f={0};ASSERT_TRUE(fixture(&f,err,sizeof(err)));
    fr_reactive_arena_request_t request[2];fr_reactive_handle_t handles[2],out[2];uint32_t needed=0;
    ASSERT_TRUE(records(request,2,0));
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,handles,&needed,err,sizeof(err)));
    fr_reactive_record_t originals[2]={request[0].record,request[1].record};
    request[0]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_RETIRE,.handle=handles[0]};
    request[1]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_RETIRE,.handle={handles[1].index,handles[1].generation+1u}};
    ASSERT_TRUE(!fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,out,&needed,err,sizeof(err)));
    request[1]=request[0];
    ASSERT_TRUE(!fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,out,&needed,err,sizeof(err)));
    for(uint32_t i=0;i<2;i++) {
        fr_reactive_record_t record;bool found=false;
        ASSERT_TRUE(fr_reactive_arena_read(&f.gpu,&f.arena,handles[i],&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
        ASSERT_TRUE(memcmp(&record,&originals[i],sizeof(record))==0);
    }
    fr_reactive_arena_stats_t stats;ASSERT_TRUE(fr_reactive_arena_stats(&f.gpu,&f.arena,&stats));
    ASSERT_EQ(stats.live,2u);ASSERT_EQ(stats.free,62u);cleanup(&f);PASS();
}
static void test_updates_keep_identity_and_do_not_partially_publish_with_a_stale_peer(void) {
    char err[256]={0};fixture_t f={0};ASSERT_TRUE(fixture(&f,err,sizeof(err)));
    fr_reactive_arena_request_t request[2];fr_reactive_handle_t handles[2],out[2];uint32_t needed=0;
    ASSERT_TRUE(records(request,2,0));
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,handles,&needed,err,sizeof(err)));
    fr_reactive_record_t expected=request[0].record;expected.state.thermal_energy+=11;
    request[0]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_UPDATE,.handle=handles[0],.record=expected};
    ASSERT_TRUE(fr_reactive_arena_apply(&f.gpu,&f.arena,request,1,out,&needed,err,sizeof(err)));
    ASSERT_EQ(out[0].index,handles[0].index);ASSERT_EQ(out[0].generation,handles[0].generation);
    request[0].record.state.thermal_energy+=7;
    request[1]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_RETIRE,.handle={handles[1].index,handles[1].generation+1u}};
    ASSERT_TRUE(!fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,out,&needed,err,sizeof(err)));
    request[1]=request[0];
    ASSERT_TRUE(!fr_reactive_arena_apply(&f.gpu,&f.arena,request,2,out,&needed,err,sizeof(err)));
    request[0].record.state.origin_id_low=1000;
    ASSERT_TRUE(!fr_reactive_arena_apply(&f.gpu,&f.arena,request,1,out,&needed,err,sizeof(err)));
    fr_reactive_record_t record;bool found=false;
    ASSERT_TRUE(fr_reactive_arena_read(&f.gpu,&f.arena,handles[0],&record,&found,err,sizeof(err)));ASSERT_TRUE(found);
    ASSERT_TRUE(memcmp(&record,&expected,sizeof(record))==0);cleanup(&f);PASS();
}
int main(void) {
    RUN(test_gpu_allocator_reuses_freed_slots_without_reviving_stale_handles);
    RUN(test_failed_growth_rolls_back_the_whole_gpu_batch_and_then_retries);
    RUN(test_stale_or_duplicate_retire_requests_cannot_partially_erase_a_batch);
    RUN(test_updates_keep_identity_and_do_not_partially_publish_with_a_stale_peer);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
