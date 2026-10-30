#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/reactive_state_layout.h"
#include "reactive_arena_gpu.h"
#include "reactive_gpu_internal.h"
#include "reactive_storage_gpu.h"
#include "runtime_body_internal.h"

#define REACTIVE_ROOT_MAGIC 0x46525331u
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
bool fr_gpu_reactive_reserve_import(dc_gpu_t *g,uint32_t count,char *err,uint32_t cap) {
    (void)g;(void)count;
    return error(err,cap,"Reactive bulk reservation is not implemented");
}
bool fr_gpu_reactive_read_stats(dc_gpu_t *g,fr_gpu_reactive_stats_t *out,char *err,uint32_t cap) {
    if(!g||!out) return error(err,cap,"Invalid reactive allocation diagnostic");
    if(!g->reactive) {*out=(fr_gpu_reactive_stats_t){0};return true;}
    fr_reactive_arena_stats_t stats;
    if(!fr_reactive_arena_stats(g,&g->reactive->arena,&stats)) return error(err,cap,"Reactive allocation diagnostic wait failed");
    *out=(fr_gpu_reactive_stats_t){stats.capacity,stats.live,stats.free,stats.retired};return true;
}
static void put_address(uint32_t *out,VkDeviceAddress address) {
    out[0]=(uint32_t)address;out[1]=(uint32_t)(address>>32);
}
void fr_gpu_reactive_bind_work(dc_gpu_t *g,uint32_t *h) {
    h[64]=g->reactive?REACTIVE_ROOT_MAGIC:0u;
    put_address(h+65,g->reactive?g->reactive->arena.root.address:0);
}
void fr_gpu_reactive_storage_destroy(dc_gpu_t *g,struct fr_reactive_gpu *r) {
    if(!g||!r) return;
    fr_renderer_compute_destroy(g,&r->stream_compute);
    dc_gpu_address_buffer_destroy(g,&r->stream);dc_gpu_address_buffer_destroy(g,&r->stream_marks);
    dc_gpu_address_buffer_destroy(g,&r->snapshot);
    fr_reactive_arena_destroy(g,&r->arena);
    dc_gpu_address_buffer_destroy(g,&r->cells);dc_gpu_address_buffer_destroy(g,&r->shapes);
    dc_gpu_address_buffer_destroy(g,&r->particles);
    free(r);
}
void fr_gpu_reactive_destroy(dc_gpu_t *g) {
    if(g) {fr_gpu_reactive_storage_destroy(g,g->reactive);g->reactive=NULL;}
}
bool fr_gpu_reactive_storage_create(dc_gpu_t *g,uint32_t shapes,struct fr_reactive_gpu **out,char *err,uint32_t cap) {
    if(!g||!shapes||!out) return error(err,cap,"Invalid detached reactive storage");
    struct fr_reactive_gpu *r=calloc(1,sizeof(*r));
    if(!r) return error(err,cap,"Cannot allocate reactive resource owner");
    if(!fr_reactive_arena_create(g,64u,&r->arena,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*8u,true,&r->cells,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)shapes*8u*8u*2u,true,&r->shapes,err,cap)) {
        fr_gpu_reactive_storage_destroy(g,r);return false;
    }
    uint32_t *h=r->arena.root.mapped;h[12]=shapes;
    put_address(h+6,r->cells.address);
    put_address(h+8,r->shapes.address);put_address(h+10,r->shapes.address+(VkDeviceSize)shapes*64u);
    *out=r;return true;
}
bool fr_gpu_reactive_initialize(dc_gpu_t *g,char *err,uint32_t cap) {
    if(g->reactive) return true;
    if(!fr_gpu_reactive_storage_create(g,dc_gpu_pixel_pool_capacity(g),&g->reactive,err,cap)) return false;
    fr_gpu_reactive_bind_work(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;return true;
}
bool fr_gpu_reactive_storage_particles(dc_gpu_t *g,struct fr_reactive_gpu *r,char *err,uint32_t cap) {
    if(r->particles.buffer) return true;
    VkDeviceSize bank=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_MPM_PARTICLES_PER_CHUNK*8u;
    if(!dc_gpu_address_buffer_create(g,bank*2u,true,&r->particles,err,cap)) return false;
    uint32_t *h=r->arena.root.mapped;put_address(h+14,r->particles.address);put_address(h+16,r->particles.address+bank);
    g->descriptor_revision++;return true;
}
bool fr_gpu_reactive_particle_storage(dc_gpu_t *g,char *err,uint32_t cap) {
    return fr_gpu_reactive_storage_particles(g,g->reactive,err,cap);
}
static uint32_t *entry(dc_gpu_t *g,const uint32_t *handle) {
    if(!g->reactive||!handle||!handle[1]) return NULL;
    const uint32_t *h=g->reactive->arena.root.mapped;
    if(handle[0]>=h[2]) return NULL;
    uint32_t *e=(uint32_t *)g->reactive->arena.entries.mapped+(size_t)handle[0]*FR_ENTRY_WORDS;
    return e[0]==handle[1]&&e[1]>=1u&&e[1]<=3u?e:NULL;
}
bool fr_gpu_reactive_reserve_shapes(dc_gpu_t *g,uint32_t words,char *err,uint32_t cap) {
    if(!g->reactive) return true;
    struct fr_reactive_gpu *r=g->reactive;uint32_t *h=r->arena.root.mapped,old=h[12];
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

static bool admit(dc_gpu_t *g,uint32_t *handle,const fr_reactive_owner_t *owner,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    uint64_t id=((uint64_t)state->parcel_id_high<<32)|state->parcel_id_low;
    uint64_t origin=((uint64_t)state->origin_id_high<<32)|state->origin_id_low;
    uint32_t *existing=entry(g,handle);
    bool same=existing&&existing[FR_ENTRY_STATE+2]==state->parcel_id_low&&existing[FR_ENTRY_STATE+3]==state->parcel_id_high;
    if(!same&&g->reactive&&id<=g->reactive->high_id) return error(err,cap,"Reactive parcel ID would reuse history");
    struct fr_reactive_gpu *r=g->reactive;
    fr_reactive_arena_request_t requests[2]={
        {.operation=same?FR_REACTIVE_ARENA_UPDATE:FR_REACTIVE_ARENA_ALLOCATE,
         .handle={same?handle[0]:0u,same?handle[1]:0u},.record={.owner=*owner,.state=*state}},
        {.operation=FR_REACTIVE_ARENA_RETIRE,.handle={handle[0],handle[1]}}};
    uint32_t count=existing&&!same?2u:1u,required=0;fr_reactive_handle_t output[2];
    if(!fr_reactive_arena_apply(g,&r->arena,requests,count,output,&required,err,cap)) return false;
    handle[0]=output[0].index;handle[1]=output[0].generation;
    if(r->high_id<id) r->high_id=id;
    if(r->high_id<origin) r->high_id=origin;
    return true;
}
bool fr_gpu_reactive_seed_cell(dc_gpu_t *g,uint32_t slot,uint32_t local,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!g->runtime_body_consumers||slot>=DC_GPU_CHUNK_SLOTS||local>=4096||
       !fr_reactive_state_valid(state,err,cap)) return error(err,cap,"Invalid reactive cell import");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive import wait failed");
    const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
    if(!registry[5]) return error(err,cap,"Reactive cell owner is not resident");
    if(!fr_gpu_reactive_initialize(g,err,cap)) return false;
    if(state->material==DC_MATERIAL_LEAVES&&!fr_gpu_reactive_particle_storage(g,err,cap)) return false;
    dc_chunk_coord_t coord;memcpy(&coord,registry,sizeof(coord));
    fr_reactive_owner_t owner=fr_reactive_cell_owner(coord,local);
    uint32_t *handle=(uint32_t *)g->reactive->cells.mapped+((size_t)slot*4096u+local)*2u;
    return admit(g,handle,&owner,state,err,cap);
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
static uint32_t *body_handle(dc_gpu_t *g,uint32_t id,uint32_t x,uint32_t y,
    char *err,uint32_t cap) {
    if(!g->reactive) return NULL;
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        const dc_gpu_body_record_t *b=dc_gpu_body_record(g,slot);
        if(!b->body.active||b->body.id!=id) continue;
        if(!(b->piece_count&DC_GPU_PIXEL_BODY_FLAG)&&!(b->motion_flags&DC_BODY_MOTION_THERMAL_GRID)) return NULL;
        const uint32_t *shape=(const uint32_t *)g->body_mapped+b->compound_reserved;
        if(x>=shape[6]||y>=shape[7]) return NULL;
        uint32_t tile=(y/16u)*shape[2]+x/16u,offset=shape[8u+tile];
        if(!offset) return NULL;
        uint32_t cell=(y%16u)*16u+x%16u;
        if(((shape[offset+1u+cell/8u]>>((cell%8u)*4u))&15u)==0u) return NULL;
        uint32_t word=b->compound_reserved+offset+1u+cell/8u;
        uint32_t first=dc_gpu_pixel_pool_word_offset(g)+DC_GPU_PIXEL_POOL_HEADER_WORDS;
        if(word<first) return NULL;
        uint32_t relative=word-first;
        uint32_t capacity=dc_gpu_pixel_pool_capacity(g),allocated=((uint32_t *)g->reactive->arena.root.mapped)[12];
        if(relative>=2u*capacity) {error(err,cap,"Reactive shape address is invalid");return NULL;}
        size_t location=(size_t)(relative/capacity)*allocated*8u+(relative%capacity)*8u+cell%8u;
        if((location*2u+1u)*sizeof(uint32_t)>g->reactive->shapes.bytes) return NULL;
        return (uint32_t *)g->reactive->shapes.mapped+location*2u;
    }
    return NULL;
}
bool fr_gpu_reactive_read_body(dc_gpu_t *g,uint32_t id,uint32_t x,uint32_t y,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    if(!g||!id||!out||!found) return error(err,cap,"Invalid reactive body diagnostic");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive body wait failed");
    dc_gpu_refresh_body_registry(g);*found=false;
    uint32_t *handle=body_handle(g,id,x,y,err,cap);
    uint32_t *e=entry(g,handle);
    if(e) {memcpy(&out->owner,e+FR_ENTRY_OWNER,sizeof(out->owner));memcpy(&out->state,e+FR_ENTRY_STATE,sizeof(out->state));*found=true;}
    return true;
}
bool fr_gpu_reactive_seed_body(dc_gpu_t *g,uint32_t id,uint32_t x,uint32_t y,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    if(!g||!id||!fr_reactive_state_valid(state,err,cap))
        return error(err,cap,"Invalid reactive body import");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Reactive body import wait failed");
    dc_gpu_refresh_body_registry(g);
    uint32_t *handle=body_handle(g,id,x,y,err,cap);
    uint32_t *e=entry(g,handle);
    if(!e||e[FR_ENTRY_STATE]!=state->material||
       e[FR_ENTRY_STATE+2u]!=state->parcel_id_low||
       e[FR_ENTRY_STATE+3u]!=state->parcel_id_high)
        return error(err,cap,"Reactive body parcel is absent or changed");
    fr_reactive_owner_t owner;
    memcpy(&owner,e+FR_ENTRY_OWNER,sizeof(owner));
    return admit(g,handle,&owner,state,err,cap);
}
bool fr_gpu_reactive_seed_particle(dc_gpu_t *g,uint32_t particle,
    const fr_reactive_state_t *state,char *err,uint32_t cap) {
    if(!g||!g->runtime_body_consumers||particle>=DC_GPU_CHUNK_SLOTS*DC_MPM_PARTICLES_PER_CHUNK||
       !fr_reactive_state_valid(state,err,cap)) return error(err,cap,"Invalid reactive particle import");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) return error(err,cap,"Particle import wait failed");
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    VkDeviceSize offset=(VkDeviceSize)particle*sizeof(dc_mpm_particle_t);
    VkBufferCopy copy={offset,offset,sizeof(dc_mpm_particle_t)};
    vkCmdCopyBuffer(g->command,g->particle_buffer,g->particle_staging_buffer,1,&copy);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) return error(err,cap,"Cannot inspect imported particle owner");
    const dc_mpm_particle_t *p=(const dc_mpm_particle_t *)g->particle_mapped+particle;
    if(!p->mass_fp||p->material!=state->material||(p->id_lo|p->id_hi)==0)
        return error(err,cap,"Reactive particle source is absent or has another material");
    if(!fr_gpu_reactive_initialize(g,err,cap)) return false;
    struct fr_reactive_gpu *r=g->reactive;
    if(!fr_gpu_reactive_particle_storage(g,err,cap)) return false;
    fr_reactive_owner_t owner={.kind=FR_REACTIVE_OWNER_PARTICLE,
        .key={p->id_lo,p->id_hi,state->parcel_id_low,state->parcel_id_high,0}};
    return admit(g,(uint32_t *)r->particles.mapped+(size_t)particle*2u,&owner,state,err,cap);
}
bool fr_gpu_reactive_read_particle(dc_gpu_t *g,uint32_t particle,
    fr_reactive_record_t *out,bool *found,char *err,uint32_t cap) {
    if(!g||particle>=DC_GPU_CHUNK_SLOTS*DC_MPM_PARTICLES_PER_CHUNK||!out||!found)
        return error(err,cap,"Invalid reactive particle diagnostic");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Particle diagnostic wait failed");
    uint32_t *e=g->reactive&&g->reactive->particles.buffer?
        entry(g,(uint32_t *)g->reactive->particles.mapped+(size_t)particle*2u):NULL;
    if(e) {memcpy(&out->owner,e+FR_ENTRY_OWNER,sizeof(out->owner));memcpy(&out->state,e+FR_ENTRY_STATE,sizeof(out->state));}
    *found=e!=NULL;return true;
}
