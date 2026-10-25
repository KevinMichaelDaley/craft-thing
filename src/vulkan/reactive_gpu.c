#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/reactive_state_layout.h"
#include "reactive_gpu_internal.h"
#include "runtime_body_internal.h"

#define REACTIVE_ROOT_MAGIC 0x46525331u
struct fr_reactive_gpu {
    dc_gpu_address_buffer_t root,entries,cells,shapes;
    uint64_t high_id;
};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void put_address(uint32_t *out,VkDeviceAddress address) {
    out[0]=(uint32_t)address;out[1]=(uint32_t)(address>>32);
}
void fr_gpu_reactive_bind_work(dc_gpu_t *g,uint32_t *h) {
    h[64]=g->reactive?REACTIVE_ROOT_MAGIC:0u;
    put_address(h+65,g->reactive?g->reactive->root.address:0);
}
void fr_gpu_reactive_destroy(dc_gpu_t *g) {
    if(!g||!g->reactive) return;
    struct fr_reactive_gpu *r=g->reactive;
    dc_gpu_address_buffer_destroy(g,&r->root);dc_gpu_address_buffer_destroy(g,&r->entries);
    dc_gpu_address_buffer_destroy(g,&r->cells);dc_gpu_address_buffer_destroy(g,&r->shapes);
    free(r);g->reactive=NULL;
}
static bool initialize(dc_gpu_t *g,char *err,uint32_t cap) {
    if(g->reactive) return true;
    struct fr_reactive_gpu *r=calloc(1,sizeof(*r));
    if(!r) return error(err,cap,"Cannot allocate reactive resource owner");
    g->reactive=r;
    uint32_t shapes=dc_gpu_pixel_pool_capacity(g);
    if(!dc_gpu_address_buffer_create(g,256,true,&r->root,err,cap)||
       !dc_gpu_address_buffer_create(g,64u*FR_ENTRY_WORDS*4u,true,&r->entries,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*8u,true,&r->cells,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)shapes*8u*8u*2u,true,&r->shapes,err,cap)) {
        fr_gpu_reactive_destroy(g);return false;
    }
    uint32_t *h=r->root.mapped;h[0]=REACTIVE_ROOT_MAGIC;h[1]=FR_REACTIVE_STATE_ABI;h[2]=64;h[12]=shapes;
    put_address(h+4,r->entries.address);put_address(h+6,r->cells.address);
    put_address(h+8,r->shapes.address);put_address(h+10,r->shapes.address+(VkDeviceSize)shapes*64u);
    fr_gpu_reactive_bind_work(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
    return true;
}
static bool grow_entries(dc_gpu_t *g,char *err,uint32_t cap) {
    struct fr_reactive_gpu *r=g->reactive;uint32_t *h=r->root.mapped;
    if(h[3]<h[2]) return true;
    if(h[2]>UINT32_MAX/2u) return error(err,cap,"Reactive arena address range exhausted");
    uint32_t next=h[2]*2u;dc_gpu_address_buffer_t pending={0};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)next*FR_ENTRY_WORDS*4u,true,&pending,err,cap)) return false;
    if(!fr_renderer_begin(g)) goto fail;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    VkBufferCopy copy={0,0,r->entries.bytes};vkCmdCopyBuffer(g->command,r->entries.buffer,pending.buffer,1,&copy);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    if(!fr_renderer_submit(g)) goto fail;
    dc_gpu_address_buffer_destroy(g,&r->entries);r->entries=pending;
    h[2]=next;put_address(h+4,pending.address);g->descriptor_revision++;return true;
fail:
    dc_gpu_address_buffer_destroy(g,&pending);return error(err,cap,"Reactive arena growth failed");
}
static uint32_t *entry(dc_gpu_t *g,const uint32_t *handle) {
    if(!g->reactive||!handle[1]) return NULL;
    const uint32_t *h=g->reactive->root.mapped;
    if(handle[0]>=h[2]) return NULL;
    uint32_t *e=(uint32_t *)g->reactive->entries.mapped+(size_t)handle[0]*FR_ENTRY_WORDS;
    return e[0]==handle[1]&&e[1]?e:NULL;
}
bool fr_gpu_reactive_reserve_shapes(dc_gpu_t *g,uint32_t words,char *err,uint32_t cap) {
    if(!g->reactive) return true;
    struct fr_reactive_gpu *r=g->reactive;uint32_t *h=r->root.mapped,old=h[12];
    if(words<=old) return true;
    dc_gpu_address_buffer_t pending={0};VkDeviceSize bank=(VkDeviceSize)old*64u;
    VkDeviceSize next_bank=(VkDeviceSize)words*64u;
    if(!dc_gpu_address_buffer_create(g,next_bank*2u,true,&pending,err,cap)) return false;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) goto fail;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    VkBufferCopy regions[2]={{0,0,bank},{bank,next_bank,bank}};
    vkCmdCopyBuffer(g->command,r->shapes.buffer,pending.buffer,2,regions);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    if(!fr_renderer_submit(g)) goto fail;
    dc_gpu_address_buffer_destroy(g,&r->shapes);r->shapes=pending;
    h[12]=words;put_address(h+8,pending.address);put_address(h+10,pending.address+next_bank);
    g->descriptor_revision++;return true;
fail:
    dc_gpu_address_buffer_destroy(g,&pending);return error(err,cap,"Reactive geometry bank growth failed");
}

bool fr_gpu_reactive_seed_cell(dc_gpu_t *g,uint32_t slot,uint32_t local,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||slot>=DC_GPU_CHUNK_SLOTS||local>=4096||
       !fr_reactive_state_valid(state,err,cap)) return error(err,cap,"Invalid reactive cell import");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive import wait failed");
    const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
    if(!registry[5]) return error(err,cap,"Reactive cell owner is not resident");
    uint64_t id=((uint64_t)state->parcel_id_high<<32)|state->parcel_id_low;
    uint32_t *handle=g->reactive?(uint32_t *)g->reactive->cells.mapped+((size_t)slot*4096u+local)*2u:NULL;
    uint32_t *existing=handle?entry(g,handle):NULL;
    bool same=existing&&existing[FR_ENTRY_STATE+2]==state->parcel_id_low&&existing[FR_ENTRY_STATE+3]==state->parcel_id_high;
    if(!same&&g->reactive&&id<=g->reactive->high_id) return error(err,cap,"Reactive parcel ID would reuse history");
    if(!initialize(g,err,cap)||(!same&&!grow_entries(g,err,cap))) return false;
    struct fr_reactive_gpu *r=g->reactive;uint32_t *h=r->root.mapped;
    handle=(uint32_t *)r->cells.mapped+((size_t)slot*4096u+local)*2u;
    /* Growth can relocate entries; resolve again after publishing the new arena. */
    existing=entry(g,handle);
    uint32_t index=same?handle[0]:h[3]++;
    uint32_t *e=(uint32_t *)r->entries.mapped+(size_t)index*FR_ENTRY_WORDS;
    if(!same) {if(existing) {existing[0]++;existing[1]=0;}memset(e,0,FR_ENTRY_WORDS*4u);e[0]=1;}
    dc_chunk_coord_t coord;memcpy(&coord,registry,sizeof(coord));
    fr_reactive_owner_t owner=fr_reactive_cell_owner(coord,local);
    memcpy(e+FR_ENTRY_OWNER,&owner,sizeof(owner));memcpy(e+FR_ENTRY_STATE,state,sizeof(*state));
    handle[0]=index;handle[1]=e[0];
    uint64_t origin=((uint64_t)state->origin_id_high<<32)|state->origin_id_low;
    if(r->high_id<id) r->high_id=id;
    if(r->high_id<origin) r->high_id=origin;
    return true;
}
bool fr_gpu_reactive_read_cell(dc_gpu_t *g,uint32_t slot,uint32_t local,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    if(!g||slot>=DC_GPU_CHUNK_SLOTS||local>=4096||!out||!found)
        return error(err,cap,"Invalid reactive cell diagnostic");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive diagnostic wait failed");
    uint32_t *e=g->reactive?entry(g,(uint32_t *)g->reactive->cells.mapped+((size_t)slot*4096u+local)*2u):NULL;
    if(e) {memcpy(&out->owner,e+FR_ENTRY_OWNER,sizeof(out->owner));memcpy(&out->state,e+FR_ENTRY_STATE,sizeof(out->state));}
    *found=e!=NULL;return true;
}
bool fr_gpu_reactive_read_body(dc_gpu_t *g,uint32_t id,uint32_t x,uint32_t y,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    if(!g||!id||!out||!found) return error(err,cap,"Invalid reactive body diagnostic");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive body wait failed");
    dc_gpu_refresh_body_registry(g);*found=false;
    if(!g->reactive) return true;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        const dc_gpu_body_record_t *b=dc_gpu_body_record(g,slot);
        if(!b->body.active||b->body.id!=id) continue;
        if(!(b->piece_count&DC_GPU_PIXEL_BODY_FLAG)) return true;
        const uint32_t *shape=(const uint32_t *)g->body_mapped+b->compound_reserved;
        if(x>=shape[6]||y>=shape[7]) return true;
        uint32_t tile=(y/16u)*shape[2]+x/16u,offset=shape[8u+tile];
        if(!offset) return true;
        uint32_t cell=(y%16u)*16u+x%16u;
        if(((shape[offset+1u+cell/8u]>>((cell%8u)*4u))&15u)==0u) return true;
        uint32_t word=b->compound_reserved+offset+1u+cell/8u;
        uint32_t relative=word-dc_gpu_pixel_pool_word_offset(g)-DC_GPU_PIXEL_POOL_HEADER_WORDS;
        uint32_t capacity=dc_gpu_pixel_pool_capacity(g),allocated=((uint32_t *)g->reactive->root.mapped)[12];
        if(relative>=2u*capacity) return error(err,cap,"Reactive shape address is invalid");
        size_t location=(size_t)(relative/capacity)*allocated*8u+(relative%capacity)*8u+cell%8u;
        uint32_t *e=entry(g,(uint32_t *)g->reactive->shapes.mapped+location*2u);
        if(e) {memcpy(&out->owner,e+FR_ENTRY_OWNER,sizeof(out->owner));memcpy(&out->state,e+FR_ENTRY_STATE,sizeof(out->state));*found=true;}
        return true;
    }
    return true;
}
bool fr_gpu_reactive_seed_particle(dc_gpu_t *g,uint32_t particle,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    (void)g;(void)particle;(void)state;(void)err;(void)cap;return false;
}
bool fr_gpu_reactive_read_particle(dc_gpu_t *g,uint32_t particle,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    (void)g;(void)particle;(void)out;(void)found;(void)err;(void)cap;return false;
}
