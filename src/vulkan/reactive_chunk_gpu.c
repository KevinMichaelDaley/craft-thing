#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "dungeoncraft/reactive_state_layout.h"
#include "reactive_gpu_internal.h"
#include "reactive_storage_gpu.h"

enum {ROWS=DC_CHUNK_CELLS+DC_MPM_PARTICLES_PER_CHUNK,ATTACHMENT_WORDS=35,
      HANDLE_OFFSET=4+ROWS*ATTACHMENT_WORDS,STREAM_WORDS=HANDLE_OFFSET+ROWS*2};
_Static_assert(sizeof(fr_reactive_attachment_t)==ATTACHMENT_WORDS*4u,"Chunk attachment ABI changed");
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void address(uint32_t *out,VkDeviceAddress value) {out[0]=(uint32_t)value;out[1]=(uint32_t)(value>>32);}
static bool resident(dc_gpu_t *g,uint32_t slot,const dc_chunk_t *chunk,char *err,uint32_t cap) {
    if(!g||!chunk||!g->runtime_body_consumers||slot>=DC_GPU_CHUNK_SLOTS||vkQueueWaitIdle(g->queue)!=VK_SUCCESS)
        return error(err,cap,"Invalid reactive chunk boundary");
    const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
    return (registry[5]&&memcmp(registry,&chunk->coord,sizeof(chunk->coord))==0)||
        error(err,cap,"Reactive chunk is not resident at this physical slot");
}
bool fr_gpu_reactive_storage_stream(dc_gpu_t *g,struct fr_reactive_gpu *r,char *err,uint32_t cap) {
    if(!g||!r) return error(err,cap,"Invalid reactive transfer storage");
    if(!r->stream.buffer&&!dc_gpu_address_buffer_create(g,(VkDeviceSize)STREAM_WORDS*4u,true,&r->stream,err,cap)) return false;
    VkDeviceSize marks=(VkDeviceSize)r->arena.capacity*4u;
    if(r->stream_marks.bytes<marks) {
        dc_gpu_address_buffer_t pending={0};
        if(!dc_gpu_address_buffer_create(g,marks,true,&pending,err,cap)) return false;
        dc_gpu_address_buffer_destroy(g,&r->stream_marks);r->stream_marks=pending;
    }
    uint32_t *root=r->arena.root.mapped;address(root+26,r->stream.address);address(root+28,r->stream_marks.address);
    VkDescriptorBufferInfo buffers[3]={{r->arena.root.buffer,0,r->arena.root.bytes},
        {g->chunk_buffer,0,VK_WHOLE_SIZE},{g->particle_buffer,0,VK_WHOLE_SIZE}};
    if(!r->stream_compute.pipeline&&!fr_renderer_compute_init(g,&r->stream_compute,buffers,3,16,
        "build/shaders/reactive_chunk_state.comp.spv",err,cap)) return false;
    VkWriteDescriptorSet writes[3];memset(writes,0,sizeof(writes));
    for(uint32_t i=0;i<3;i++) writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet=r->stream_compute.descriptor,.dstBinding=i,.descriptorCount=1,
        .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=buffers+i};
    vkUpdateDescriptorSets(g->device,3,writes,0,NULL);return true;
}
static bool resources(dc_gpu_t *g,char *err,uint32_t cap) {
    return fr_gpu_reactive_storage_stream(g,g->reactive,err,cap);
}
static bool dispatch(dc_gpu_t *g,uint32_t slot,uint32_t count,uint32_t first,uint32_t last,char *err,uint32_t cap) {
    struct fr_reactive_gpu *r=g->reactive;fr_renderer_compute_t *compute=&r->stream_compute;
    uint32_t maximum=ROWS;
    if(((first==1u&&((uint32_t *)r->stream.mapped)[2]==0u)||first==5u)&&
       r->arena.capacity>maximum) maximum=r->arena.capacity;
    uint32_t groups=(maximum-1u)/64u+1u;VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(g->physical,&props);
    uint32_t x=groups<props.limits.maxComputeWorkGroupCount[0]?groups:props.limits.maxComputeWorkGroupCount[0];
    uint32_t y=(groups-1u)/x+1u;
    if(y>props.limits.maxComputeWorkGroupCount[1]||!fr_renderer_begin(g)) return error(err,cap,"Cannot dispatch reactive chunk boundary");
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT);
    if(first==1u&&((uint32_t *)r->stream.mapped)[2]==0u)
        vkCmdFillBuffer(g->command,r->stream_marks.buffer,0,r->stream_marks.bytes,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute->pipeline_layout,0,1,&compute->descriptor,0,NULL);
    for(uint32_t phase=first;phase<=last;phase++) {
        uint32_t push[4]={phase,slot,count,0};
        vkCmdPushConstants(g->command,compute->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(g->command,x,y,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Reactive chunk boundary submission failed");
}
bool fr_gpu_reactive_clear_chunk(dc_gpu_t *g,uint32_t slot,char *err,uint32_t cap) {
    if(!g||slot>=DC_GPU_CHUNK_SLOTS) return error(err,cap,"Invalid reactive chunk retirement");
    if(!g->reactive) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!resources(g,err,cap)) return false;
    return dispatch(g,slot,0,3,3,err,cap);
}
bool fr_gpu_reactive_export_bodies(dc_gpu_t *g,fr_reactive_record_t **records,
    uint32_t *count,uint64_t *next,char *err,uint32_t cap) {
    if(!g||!records||!count||!next||vkQueueWaitIdle(g->queue)!=VK_SUCCESS)
        return error(err,cap,"Invalid reactive body save boundary");
    if(!g->reactive) {*records=NULL;*count=0;*next=1;return true;}
    struct fr_reactive_gpu *r=g->reactive;
    if(r->high_id==UINT64_MAX||!resources(g,err,cap)) return error(err,cap,"Cannot preflight reactive body snapshot");
    VkDeviceSize size=16u+(VkDeviceSize)r->arena.capacity*sizeof(fr_reactive_record_t);
    if(r->snapshot.bytes<size) {
        dc_gpu_address_buffer_t pending={0};
        if(!dc_gpu_address_buffer_create(g,size,true,&pending,err,cap)) return false;
        dc_gpu_address_buffer_destroy(g,&r->snapshot);r->snapshot=pending;
    }
    address((uint32_t *)r->arena.root.mapped+30,r->snapshot.address);
    memset(r->snapshot.mapped,0,16);
    if(!dispatch(g,0,0,5,5,err,cap)) return false;
    uint32_t total=((uint32_t *)r->snapshot.mapped)[0];
    if(total>r->arena.capacity) return error(err,cap,"Reactive body snapshot overflow");
    fr_reactive_record_t *copy=total?malloc((size_t)total*sizeof(*copy)):NULL;
    if(total&&!copy) return error(err,cap,"Cannot receive immutable body snapshot");
    if(total) memcpy(copy,(uint32_t *)r->snapshot.mapped+4,(size_t)total*sizeof(*copy));
    *records=copy;*count=total;*next=r->high_id+1u;return true;
}
bool fr_gpu_reactive_export_chunk(dc_gpu_t *g,uint32_t slot,const dc_chunk_t *chunk,
    fr_reactive_chunk_t *out,char *err,uint32_t cap) {
    if(!out||!resident(g,slot,chunk,err,cap)) return false;
    if(!g->reactive) {*out=(fr_reactive_chunk_t){.next_id=1};return true;}
    struct fr_reactive_gpu *r=g->reactive;
    if(r->high_id==UINT64_MAX||!resources(g,err,cap)) return error(err,cap,"Reactive save counter/resource failure");
    memset(r->stream.mapped,0,16);
    if(!dispatch(g,slot,0,0,0,err,cap)) return false;
    const uint32_t *words=r->stream.mapped;uint32_t count=words[0];
    if(count>ROWS||words[1]) return error(err,cap,"Reactive GPU compaction bounds failure");
    fr_reactive_chunk_t result={.count=count,.next_id=r->high_id+1u};
    if(count) {
        result.attachments=malloc((size_t)count*sizeof(*result.attachments));
        if(!result.attachments) return error(err,cap,"Cannot receive immutable chunk snapshot");
        memcpy(result.attachments,words+4,(size_t)count*sizeof(*result.attachments));
    }
    if(!fr_reactive_chunk_valid(&result,chunk,err,cap)) {fr_reactive_chunk_release(&result);return false;}
    *out=result;return true;
}
static int parcel_order(const void *a,const void *b) {
    const fr_reactive_attachment_t *x=a,*y=b;
    uint64_t first=(uint64_t)x->record.state.parcel_id_low|((uint64_t)x->record.state.parcel_id_high<<32);
    uint64_t second=(uint64_t)y->record.state.parcel_id_low|((uint64_t)y->record.state.parcel_id_high<<32);
    return first<second?-1:first>second?1:0;
}
bool fr_gpu_reactive_bind_prepared_chunk(dc_gpu_t *g,uint32_t slot,
    const fr_reactive_chunk_t *channel,const uint32_t *handles,char *err,uint32_t cap) {
    if(!g||!g->reactive||!channel||slot>=DC_GPU_CHUNK_SLOTS||channel->count>ROWS||
       (channel->count&&!handles)||!g->reactive->stream_compute.pipeline)
        return error(err,cap,"Invalid prepared checkpoint chunk binding");
    struct fr_reactive_gpu *r=g->reactive;
    memset(r->stream.mapped,0,16);
    if(channel->count) {
        memcpy((uint32_t *)r->stream.mapped+4,channel->attachments,(size_t)channel->count*sizeof(*channel->attachments));
        memcpy((uint32_t *)r->stream.mapped+HANDLE_OFFSET,handles,(size_t)channel->count*8u);
        return dispatch(g,slot,channel->count,4,4,err,cap);
    }
    return true;
}
bool fr_gpu_reactive_import_chunk(dc_gpu_t *g,uint32_t slot,const dc_chunk_t *chunk,
    const fr_reactive_chunk_t *channel,char *err,uint32_t cap) {
    Uint64 started=SDL_GetTicks64(),validated=0,prepared=0,checked=0,allocated=0;
    if(!resident(g,slot,chunk,err,cap)) return false;
    fr_reactive_chunk_t ambient={.next_id=g->reactive?g->reactive->high_id+1u:1u};
    if(!channel) channel=&ambient;
    if(!fr_reactive_chunk_valid(channel,chunk,err,cap)) return false;
    validated=SDL_GetTicks64();
    if(!g->reactive&&channel->count==0u&&channel->next_id==1u) return true;
    if(!fr_gpu_reactive_initialize(g,err,cap)) return false;
    struct fr_reactive_gpu *r=g->reactive;
    for(uint32_t i=0;i<channel->count;i++)
        if((channel->attachments[i].record.owner.kind==FR_REACTIVE_OWNER_PARTICLE||
            channel->attachments[i].record.state.material==DC_MATERIAL_LEAVES)&&
           !fr_gpu_reactive_particle_storage(g,err,cap)) return false;
    if(!resources(g,err,cap)) return false;
    prepared=SDL_GetTicks64();
    uint32_t count=channel->count;fr_reactive_attachment_t *sorted=NULL;
    fr_reactive_arena_request_t *requests=NULL;fr_reactive_handle_t *handles=NULL;
    if(count) {
        sorted=malloc((size_t)count*sizeof(*sorted));requests=calloc(count,sizeof(*requests));handles=malloc((size_t)count*sizeof(*handles));
        if(!sorted||!requests||!handles) {free(sorted);free(requests);free(handles);return error(err,cap,"Cannot stage immutable reactive import");}
        memcpy(sorted,channel->attachments,(size_t)count*sizeof(*sorted));
        qsort(sorted,count,sizeof(*sorted),parcel_order);
        for(uint32_t i=0;i<count;i++) requests[i]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_ALLOCATE,.record=sorted[i].record};
    }
    memset(r->stream.mapped,0,16);
    ((uint32_t *)r->stream.mapped)[2]=1u;
    if(count) {
        const fr_reactive_attachment_t *first=sorted;
        uint64_t first_id=(uint64_t)first->record.state.parcel_id_low|
            ((uint64_t)first->record.state.parcel_id_high<<32u);
        ((uint32_t *)r->stream.mapped)[2]=first_id>r->high_id?1u:0u;
    }
    if(count) memcpy((uint32_t *)r->stream.mapped+4,sorted,(size_t)count*sizeof(*sorted));
    free(sorted);
    bool okay=dispatch(g,slot,count,1,2,err,cap);
    checked=SDL_GetTicks64();
    uint32_t status=((uint32_t *)r->stream.mapped)[1];
    if(okay&&status) okay=error(err,cap,status&1u?"Reactive import duplicates a resident parcel identity":"Reactive import differs from actual GPU physics");
    uint32_t required=0;
    if(okay&&count) okay=fr_reactive_arena_apply(g,&r->arena,requests,count,handles,&required,err,cap);
    allocated=SDL_GetTicks64();
    free(requests);
    if(okay) {
        if(count) memcpy((uint32_t *)r->stream.mapped+HANDLE_OFFSET,handles,(size_t)count*sizeof(*handles));
        okay=dispatch(g,slot,count,3,4,err,cap);
        if(okay&&channel->next_id-1u>r->high_id) r->high_id=channel->next_id-1u;
    }
    free(handles);
    if(getenv("DC_REACTIVE_IMPORT_TRACE")) fprintf(stderr,
        "Reactive import slot%u count%u disjoint%u valid%llums prep%llums check%llums arena%llums bind%llums\n",
        slot,count,((uint32_t *)r->stream.mapped)[2],
        (unsigned long long)(validated-started),(unsigned long long)(prepared-validated),
        (unsigned long long)(checked-prepared),(unsigned long long)(allocated-checked),
        (unsigned long long)(SDL_GetTicks64()-allocated));
    return okay;
}
