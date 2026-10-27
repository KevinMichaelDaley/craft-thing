#include <stdio.h>
#include <string.h>
#include "dungeoncraft/reactive_arena_layout.h"
#include "dungeoncraft/reactive_state_layout.h"
#include "reactive_arena_gpu.h"

_Static_assert(sizeof(fr_reactive_arena_request_t)==FR_ARENA_REQUEST_INPUT_WORDS*4u,"Arena request ABI changed");
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static uint32_t rounded(uint32_t current,uint32_t required) {
    uint32_t next=current?current:64u;
    while(next<required) {if(next>UINT32_MAX/2u) return required;next*=2u;}
    return next;
}
static void address(uint32_t *out,VkDeviceAddress value) {out[0]=(uint32_t)value;out[1]=(uint32_t)(value>>32);}
static VkDeviceSize bit_bytes(uint32_t capacity) {return ((VkDeviceSize)capacity+31u)/32u*4u;}
static void transfer_barrier(dc_gpu_t *g) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
}
static void publish_barrier(dc_gpu_t *g) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}
void fr_reactive_arena_destroy(dc_gpu_t *g,fr_reactive_arena_gpu_t *a) {
    if(!g||!a) return;
    fr_renderer_compute_destroy(g,&a->compute);
    dc_gpu_address_buffer_destroy(g,&a->root);dc_gpu_address_buffer_destroy(g,&a->entries);
    dc_gpu_address_buffer_destroy(g,&a->free_bits);dc_gpu_address_buffer_destroy(g,&a->mailbox);
    dc_gpu_address_buffer_destroy(g,&a->requests);dc_gpu_address_buffer_destroy(g,&a->thermal_scratch);*a=(fr_reactive_arena_gpu_t){0};
}
bool fr_reactive_arena_create(dc_gpu_t *g,uint32_t initial,fr_reactive_arena_gpu_t *out,char *err,uint32_t cap) {
    if(!g||!initial||!out||out->root.buffer) return error(err,cap,"Invalid reactive arena destination");
    fr_reactive_arena_gpu_t a={.capacity=rounded(0,initial),.request_capacity=64};
    if(!dc_gpu_address_buffer_create(g,256,true,&a.root,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)a.capacity*FR_ENTRY_WORDS*4u,true,&a.entries,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)a.capacity*16u,false,&a.thermal_scratch,err,cap)||
       !dc_gpu_address_buffer_create(g,bit_bytes(a.capacity),true,&a.free_bits,err,cap)||
       !dc_gpu_address_buffer_create(g,64,true,&a.mailbox,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)a.request_capacity*FR_ARENA_REQUEST_WORDS*4u,true,&a.requests,err,cap)) goto fail;
    uint32_t *root=a.root.mapped;root[0]=FR_ARENA_MAGIC;root[1]=FR_ARENA_GPU_ABI;root[2]=a.capacity;
    address(root+FR_ARENA_ENTRIES,a.entries.address);address(root+FR_ARENA_BITS,a.free_bits.address);
    address(root+FR_ARENA_MAILBOX,a.mailbox.address);address(root+FR_ARENA_REQUESTS,a.requests.address);
    address(root+FR_ARENA_THERMAL_SCRATCH,a.thermal_scratch.address);
    ((uint32_t *)a.mailbox.mapped)[FR_ARENA_MAIL_FREE]=a.capacity;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) goto fail;
    transfer_barrier(g);vkCmdFillBuffer(g->command,a.free_bits.buffer,0,a.free_bits.bytes,UINT32_MAX);
    if(a.capacity%32u) {
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
        uint32_t mask=(1u<<(a.capacity%32u))-1u;
        vkCmdUpdateBuffer(g->command,a.free_bits.buffer,a.free_bits.bytes-4u,4,&mask);
    }
    publish_barrier(g);if(!fr_renderer_submit(g)) goto fail;
    VkDescriptorBufferInfo buffer={a.root.buffer,0,a.root.bytes};
    if(!fr_renderer_compute_init(g,&a.compute,&buffer,1,16,"build/shaders/reactive_arena.comp.spv",err,cap)) goto fail;
    *out=a;return true;
fail:
    fr_reactive_arena_destroy(g,&a);return error(err,cap,"Cannot initialize reactive GPU arena");
}
static bool grow_requests(dc_gpu_t *g,fr_reactive_arena_gpu_t *a,uint32_t count,char *err,uint32_t cap) {
    if(count<=a->request_capacity) return true;
    uint32_t next=rounded(a->request_capacity,count);dc_gpu_address_buffer_t pending={0};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)next*FR_ARENA_REQUEST_WORDS*4u,true,&pending,err,cap)) return false;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) {dc_gpu_address_buffer_destroy(g,&pending);return false;}
    dc_gpu_address_buffer_destroy(g,&a->requests);a->requests=pending;a->request_capacity=next;
    address((uint32_t *)a->root.mapped+FR_ARENA_REQUESTS,pending.address);return true;
}
static bool grow_entries(dc_gpu_t *g,fr_reactive_arena_gpu_t *a,uint32_t required,char *err,uint32_t cap) {
    if(required<=a->capacity) return error(err,cap,"Reactive arena cannot grow beyond its address range");
    uint32_t next=rounded(a->capacity,required);dc_gpu_address_buffer_t entries={0},bits={0},scratch={0};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)next*FR_ENTRY_WORDS*4u,true,&entries,err,cap)||
       !dc_gpu_address_buffer_create(g,bit_bytes(next),true,&bits,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)next*16u,false,&scratch,err,cap)) goto fail;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) goto fail;
    transfer_barrier(g);
    VkBufferCopy region={0,0,a->entries.bytes};vkCmdCopyBuffer(g->command,a->entries.buffer,entries.buffer,1,&region);
    region.size=a->free_bits.bytes;vkCmdCopyBuffer(g->command,a->free_bits.buffer,bits.buffer,1,&region);
    if(bits.bytes>a->free_bits.bytes)
        vkCmdFillBuffer(g->command,bits.buffer,a->free_bits.bytes,bits.bytes-a->free_bits.bytes,UINT32_MAX);
    if(next%32u) {
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
        uint32_t mask=(1u<<(next%32u))-1u;vkCmdUpdateBuffer(g->command,bits.buffer,bits.bytes-4u,4,&mask);
    }
    uint32_t free=((uint32_t *)a->mailbox.mapped)[FR_ARENA_MAIL_FREE]+next-a->capacity;
    vkCmdUpdateBuffer(g->command,a->mailbox.buffer,FR_ARENA_MAIL_FREE*4u,4,&free);
    publish_barrier(g);if(!fr_renderer_submit(g)) goto fail;
    dc_gpu_address_buffer_destroy(g,&a->entries);dc_gpu_address_buffer_destroy(g,&a->free_bits);
    dc_gpu_address_buffer_destroy(g,&a->thermal_scratch);a->thermal_scratch=scratch;
    a->entries=entries;a->free_bits=bits;a->capacity=next;
    uint32_t *root=a->root.mapped;root[2]=next;
    address(root+FR_ARENA_ENTRIES,entries.address);address(root+FR_ARENA_BITS,bits.address);
    address(root+FR_ARENA_THERMAL_SCRATCH,scratch.address);
    g->descriptor_revision++;return true;
fail:
    dc_gpu_address_buffer_destroy(g,&entries);dc_gpu_address_buffer_destroy(g,&bits);dc_gpu_address_buffer_destroy(g,&scratch);return false;
}
bool fr_reactive_arena_reserve_free(dc_gpu_t *g,fr_reactive_arena_gpu_t *a,uint32_t count,char *err,uint32_t cap) {
    if(!g||!a||!a->root.buffer) return error(err,cap,"Invalid GPU producer reservation");
    VkResult waited=vkQueueWaitIdle(g->queue);
    if(waited!=VK_SUCCESS) return error(err,cap,"Cannot complete GPU producer preflight");
    uint32_t free=((uint32_t *)a->mailbox.mapped)[FR_ARENA_MAIL_FREE];
    uint32_t needed=count>a->capacity/8u?count:a->capacity/8u;
    if(free>=needed) return true;
    uint64_t required=(uint64_t)a->capacity+needed-free;
    if(required>UINT32_MAX) return error(err,cap,"Reactive arena exceeds its address range");
    return grow_entries(g,a,(uint32_t)required,err,cap);
}
static bool dispatch(dc_gpu_t *g,fr_reactive_arena_gpu_t *a,uint32_t count,char *err,uint32_t cap) {
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    uint32_t groups=(count-1u)/64u+1u,x=groups;
    if(x>properties.limits.maxComputeWorkGroupCount[0]) x=properties.limits.maxComputeWorkGroupCount[0];
    uint32_t y=(groups-1u)/x+1u;
    if(y>properties.limits.maxComputeWorkGroupCount[1]) return error(err,cap,"Reactive requests exceed device dispatch dimensions");
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin reactive arena batch");
    transfer_barrier(g);
    vkCmdFillBuffer(g->command,a->mailbox.buffer,FR_ARENA_MAIL_STATUS*4u,4,0);
    vkCmdFillBuffer(g->command,a->mailbox.buffer,FR_ARENA_MAIL_REQUIRED*4u,4,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,a->compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,a->compute.pipeline_layout,0,1,&a->compute.descriptor,0,NULL);
    for(uint32_t phase=0;phase<3;phase++) {
        uint32_t push[4]={count,phase,0,0};
        vkCmdPushConstants(g->command,a->compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(g->command,x,y,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Reactive arena batch submission failed");
}
bool fr_reactive_arena_apply(dc_gpu_t *g,fr_reactive_arena_gpu_t *arena,
    const fr_reactive_arena_request_t *requests,uint32_t count,fr_reactive_handle_t *out,
    uint32_t *required,char *err,uint32_t cap) {
    if(!g||!arena||!arena->root.buffer||!count||!requests||!out||!required)
        return error(err,cap,"Invalid reactive arena batch");
    *required=0;
    for(uint32_t i=0;i<count;i++) {
        uint32_t op=requests[i].operation;
        if(requests[i].reserved||op<FR_REACTIVE_ARENA_ALLOCATE||op>FR_REACTIVE_ARENA_RETIRE)
            return error(err,cap,"Invalid reactive arena operation");
        if(op!=FR_REACTIVE_ARENA_RETIRE&&(!fr_reactive_owner_valid(&requests[i].record.owner,err,cap)||
            !fr_reactive_state_valid(&requests[i].record.state,err,cap))) return false;
    }
    if(!grow_requests(g,arena,count,err,cap)) return false;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive arena import wait failed");
    uint32_t *staged=arena->requests.mapped;
    for(uint32_t i=0;i<count;i++) {
        uint32_t *r=staged+(size_t)i*FR_ARENA_REQUEST_WORDS;
        memcpy(r,requests+i,sizeof(*requests));memset(r+FR_ARENA_REQUEST_INPUT_WORDS,0,16);
    }
    for(;;) {
        if(!dispatch(g,arena,count,err,cap)) return false;
        const uint32_t *mail=arena->mailbox.mapped;uint32_t status=mail[FR_ARENA_MAIL_STATUS];
        if(status==0u) break;
        if(status!=FR_ARENA_STATUS_CAPACITY) return error(err,cap,"Stale, duplicate or invalid reactive arena request");
        *required=mail[FR_ARENA_MAIL_REQUIRED];
        if(!grow_entries(g,arena,*required,err,cap)) return false;
    }
    for(uint32_t i=0;i<count;i++) {
        uint32_t *r=staged+(size_t)i*FR_ARENA_REQUEST_WORDS;
        out[i]=(fr_reactive_handle_t){r[FR_ARENA_REQUEST_INDEX],r[FR_ARENA_REQUEST_GENERATION]};
    }
    return true;
}
bool fr_reactive_arena_read(dc_gpu_t *g,const fr_reactive_arena_gpu_t *arena,
    fr_reactive_handle_t handle,fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    if(!g||!arena||!arena->root.buffer||!out||!found) return error(err,cap,"Invalid reactive arena diagnostic");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive arena diagnostic wait failed");
    const uint32_t *e=handle.index<arena->capacity?
        (const uint32_t *)arena->entries.mapped+(size_t)handle.index*FR_ENTRY_WORDS:NULL;
    bool valid=e&&handle.generation&&e[FR_ENTRY_GENERATION]==handle.generation&&e[FR_ENTRY_OWNER]>=1u&&e[FR_ENTRY_OWNER]<=3u;
    if(valid) {memcpy(&out->owner,e+FR_ENTRY_OWNER,sizeof(out->owner));memcpy(&out->state,e+FR_ENTRY_STATE,sizeof(out->state));}
    *found=valid;return true;
}
bool fr_reactive_arena_stats(dc_gpu_t *g,const fr_reactive_arena_gpu_t *arena,fr_reactive_arena_stats_t *out) {
    if(!g||!arena||!arena->root.buffer||!out||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return false;
    const uint32_t *mail=arena->mailbox.mapped;
    *out=(fr_reactive_arena_stats_t){arena->capacity,mail[FR_ARENA_MAIL_LIVE],mail[FR_ARENA_MAIL_FREE],mail[FR_ARENA_MAIL_RETIRED]};
    return true;
}
