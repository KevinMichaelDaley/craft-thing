#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "body_storage_gpu.h"
#include "renderer_internal.h"
#include "runtime_body_gpu.h"
#include "runtime_body_internal.h"
static dc_body_address_t address(VkDeviceAddress a) {
    return (dc_body_address_t){(uint32_t)a,(uint32_t)(a>>32)};
}
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
uint32_t dc_gpu_body_capacity(const dc_gpu_t *g) {
    return g&&g->runtime_bodies?g->runtime_bodies->store.slab_count*DC_BODY_SLAB_RECORDS:DC_GPU_BODY_CAPACITY;
}
dc_gpu_body_record_t *dc_gpu_body_record(const dc_gpu_t *g,uint32_t slot) {
    if(!g||slot>=dc_gpu_body_capacity(g)) return NULL;
    if(slot<DC_BODY_SLAB_RECORDS) return (dc_gpu_body_record_t *)g->body_mapped+slot;
    return dc_gpu_body_store_record(&g->runtime_bodies->store,slot);
}
static bool work_create(dc_gpu_t *g,uint32_t slots,uint32_t force_groups,uint32_t fragment_cells,
                        dc_gpu_address_buffer_t *out,char *err,uint32_t cap) {
    uint64_t words=256u,bits=words;
    words+=(uint64_t)dc_gpu_broadphase_buckets(g)*((slots+31u)/32u);
    uint64_t wet=words;words+=slots;
    uint64_t partials=words;
    words+=(uint64_t)force_groups*3u;
    uint64_t ids=words;words+=(2u*(uint64_t)slots+31u)/32u;
    uint64_t hash=words,hash_capacity=1;
    while(hash_capacity<2u*(uint64_t)slots) hash_capacity*=2u;
    words+=2u*hash_capacity;
    uint64_t scan_a=words;words+=4u*(uint64_t)slots;
    uint64_t scan_b=words;words+=4u*(uint64_t)slots;
    uint64_t carve_ids=words;words+=slots;
    if(fragment_cells<slots) fragment_cells=slots;
    uint64_t fragments=words;words+=12u*(uint64_t)fragment_cells;
    if(words>UINT32_MAX||!dc_gpu_address_buffer_create(g,words*4u,true,out,err,cap))
        return error(err,cap,"Cannot allocate dynamic body work resources");
    uint32_t header[16]={slots,(uint32_t)bits,force_groups,(uint32_t)wet,
        (uint32_t)partials,(uint32_t)ids,(uint32_t)words,(uint32_t)hash,(uint32_t)hash_capacity,
        (uint32_t)scan_a,(uint32_t)scan_b,(uint32_t)carve_ids,(uint32_t)fragments,fragment_cells,slots*2u};
    memcpy(out->mapped,header,sizeof(header));
    uint32_t *h=out->mapped;h[29]=dc_gpu_pixel_pool_capacity(g);
    dc_body_address_t a=address(g->shape_color_address+dc_gpu_color_terrain_bytes());
    dc_body_address_t b=address(g->shape_color_address+dc_gpu_color_terrain_bytes()+dc_gpu_color_bank_bytes(g));
    h[32]=a.low;h[33]=a.high;h[34]=b.low;h[35]=b.high;
    h[36]=g->runtime_pair_capacity?g->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY;
    h[37]=g->runtime_contact_capacity?g->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY;
    dc_body_address_t cache=address(g->runtime_bodies->component_cache.address);
    h[42]=cache.low;h[43]=cache.high;
    return true;
}
bool dc_gpu_runtime_bodies_init(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||g->runtime_bodies) return false;
    struct dc_runtime_bodies *r=calloc(1,sizeof(*r));
    if(!r) return error(err,cap,"Cannot allocate body resource ownership");
    g->runtime_bodies=r;
    g->runtime_body_slots=DC_BODY_SLAB_RECORDS;
    r->force_groups=64u;r->fragment_cells=64u;
    r->store=(dc_gpu_body_store_t){.slab_count=1,.auxiliary_bytes=512,.solver_bytes=256,.host_records=true};
    if(!dc_gpu_address_buffer_create(g,dc_gpu_body_storage_bytes(g),true,&r->metadata,err,cap)) return false;
    g->body_buffer=r->metadata.buffer;g->body_memory=r->metadata.memory;g->body_mapped=r->metadata.mapped;
    g->body_ids=calloc(DC_BODY_SLAB_RECORDS,sizeof(*g->body_ids));
    if(!g->body_ids||
       !dc_gpu_address_buffer_create(g,sizeof(dc_body_storage_root_t),true,&r->root,err,cap)||
       !dc_gpu_address_buffer_create(g,sizeof(dc_body_storage_page_t),true,&r->store.directory,err,cap)||
       !dc_gpu_address_buffer_create(g,DC_BODY_SLAB_RECORDS*(256u+32u+512u),true,&r->initial_auxiliary,err,cap)||
       !dc_gpu_address_buffer_create(g,64u,true,&r->mailbox,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS*4u,true,&r->component_cache,err,cap)||
       !work_create(g,DC_BODY_SLAB_RECORDS,r->force_groups,r->fragment_cells,&r->work,err,cap)) return false;
    VkDeviceAddress data=r->metadata.address+DC_BODY_SLAB_RECORDS*sizeof(dc_gpu_body_record_t);
    dc_body_storage_page_t page={.records=address(r->metadata.address),
        .solver=address(r->initial_auxiliary.address),
        .ownership=address(r->initial_auxiliary.address+DC_BODY_SLAB_RECORDS*256u),
        .auxiliary=address(r->initial_auxiliary.address+DC_BODY_SLAB_RECORDS*(256u+32u)),
        .free_mask={UINT32_MAX,UINT32_MAX},.generation=1};
    memcpy(r->store.directory.mapped,&page,sizeof(page));
    dc_body_storage_root_t root={.directory=address(r->store.directory.address),
        .slab_shift=DC_BODY_SLAB_SHIFT,.abi_version=DC_BODY_STORAGE_ABI,.generation=1,
        .slab_count=1,.directory_capacity=1,.data=address(data),.geometry=address(r->metadata.address),
        .work=address(r->work.address),.requests=address(r->mailbox.address)};
    memcpy(r->root.mapped,&root,sizeof(root));
    VkDescriptorBufferInfo buffer={r->root.buffer,0,sizeof(root)};
    if(!fr_renderer_compute_init(g,&r->initialize,&buffer,1,48,
        "build/diagnostics/body_storage_init.spv",err,cap)) return false;
    VkDescriptorBufferInfo index_buffers[5]={buffer,buffer,buffer,buffer,buffer};
    if(!fr_renderer_compute_init(g,&r->index,index_buffers,5,16,
        "build/shaders/runtime_body_index.comp.spv",err,cap)) return false;
    if(!fr_renderer_compute_init(g,&r->scan,index_buffers,5,16,
        "build/shaders/runtime_body_scan.comp.spv",err,cap)) return false;
    r->enabled=true;
    g->runtime_body_consumers=r->enabled;
    return true;
}
void dc_gpu_runtime_bodies_destroy(dc_gpu_t *g) {
    if(!g||!g->runtime_bodies) return;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    fr_renderer_compute_destroy(g,&r->initialize);
    fr_renderer_compute_destroy(g,&r->index);
    fr_renderer_compute_destroy(g,&r->scan);
    dc_gpu_body_store_destroy(g,&r->store);
    dc_gpu_address_buffer_destroy(g,&r->initial_auxiliary);
    dc_gpu_address_buffer_destroy(g,&r->work);
    dc_gpu_address_buffer_destroy(g,&r->component_cache);
    dc_gpu_address_buffer_destroy(g,&r->mailbox);
    dc_gpu_address_buffer_destroy(g,&r->root);
    /* The existing body-buffer owner releases this aliased metadata allocation. */
    free(r);g->runtime_bodies=NULL;
}
bool dc_gpu_runtime_bodies_reserve(dc_gpu_t *g,uint32_t slots,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!g->command) return error(err,cap,"Body growth requires an initialized context");
    uint32_t previous=dc_gpu_body_capacity(g);
    if(slots<=previous) return true;
    if(slots==DC_BODY_SLOT_NONE||previous>UINT32_MAX/2u) return error(err,cap,"Body address range exhausted");
    if(slots<previous*2u) slots=previous*2u;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    dc_gpu_body_growth_t growth={0};dc_gpu_address_buffer_t work={0};
    if(!dc_gpu_body_growth_prepare(g,&r->store,slots,&growth,err,cap)) return false;
    uint32_t allocated=growth.slab_count*DC_BODY_SLAB_RECORDS;
    uint32_t *ids=calloc(allocated,sizeof(*ids));
    if(!ids||!work_create(g,allocated,r->force_groups,r->fragment_cells,&work,err,cap)) {
        free(ids);dc_gpu_body_growth_discard(g,&growth);return false;
    }
    memcpy(ids,g->body_ids,(size_t)previous*sizeof(*ids));
    dc_body_storage_root_t original=*(dc_body_storage_root_t *)r->root.mapped;
    bool okay=vkQueueWaitIdle(g->queue)==VK_SUCCESS&&fr_renderer_begin(g);
    if(okay) {
        vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,
            r->initialize.pipeline_layout,0,1,&r->initialize.descriptor,0,NULL);
        okay=dc_gpu_body_growth_record(g->command,r->root.buffer,r->initialize.pipeline_layout,
            r->initialize.pipeline,&r->store,&growth,++r->submission);
    }
    if(okay) okay=fr_renderer_submit(g)&&dc_gpu_body_growth_finish(g,&r->store,&growth,r->submission);
    if(!okay) {
        vkDeviceWaitIdle(g->device);
        *(dc_body_storage_root_t *)r->root.mapped=original;
        growth.submission=0;
        free(ids);dc_gpu_address_buffer_destroy(g,&work);
        dc_gpu_body_growth_discard(g,&growth);return error(err,cap,"Cannot complete body resource growth");
    }
    dc_gpu_address_buffer_destroy(g,&r->work);r->work=work;
    if(r->fragment_cells<allocated) r->fragment_cells=allocated;
    ((dc_body_storage_root_t *)r->root.mapped)->work=address(work.address);
    free(g->body_ids);g->body_ids=ids;
    g->runtime_body_slots=allocated;
    return true;
}
bool dc_gpu_runtime_bodies_service(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!g->runtime_bodies->enabled) return true;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    const uint32_t *mail=r->mailbox.mapped;
    if(!dc_gpu_runtime_queues_reserve(g,mail[6],mail[7],err,cap)) return false;
    if(mail[11]>dc_gpu_pixel_pool_capacity(g)&&
       !dc_gpu_runtime_geometry_reserve(g,mail[11],err,cap)) return false;
    if(mail[2]>dc_gpu_body_capacity(g)&&!dc_gpu_runtime_bodies_reserve(g,mail[2],err,cap)) return false;
    uint32_t force=r->force_groups,fragment=r->fragment_cells;
    while(force<mail[8]) {if(force>UINT32_MAX/2u) return error(err,cap,"Hydrodynamic work address range exhausted");force*=2u;}
    while(fragment<mail[9]) {if(fragment>UINT32_MAX/2u) return error(err,cap,"Fracture work address range exhausted");fragment*=2u;}
    if(force==r->force_groups&&fragment==r->fragment_cells) return true;
    dc_gpu_address_buffer_t work={0};
    if(!work_create(g,dc_gpu_body_capacity(g),force,fragment,&work,err,cap)) return false;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) {dc_gpu_address_buffer_destroy(g,&work);return error(err,cap,"Cannot reach body-work growth boundary");}
    dc_gpu_address_buffer_destroy(g,&r->work);r->work=work;
    r->force_groups=force;r->fragment_cells=fragment;
    ((dc_body_storage_root_t *)r->root.mapped)->work=address(work.address);
    return true;
}
VkDescriptorBufferInfo dc_gpu_runtime_bodies_binding(const dc_gpu_t *g) {
    if(g&&g->runtime_bodies&&g->runtime_bodies->enabled)
        return (VkDescriptorBufferInfo){g->runtime_bodies->root.buffer,0,sizeof(dc_body_storage_root_t)};
    return (VkDescriptorBufferInfo){g->body_buffer,0,dc_gpu_body_storage_bytes(g)};
}
void dc_gpu_runtime_record_body_index(dc_gpu_t *g) {
    if(!g||!g->runtime_body_consumers) return;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline_layout,0,1,&r->index.descriptor,0,NULL);
    for(uint32_t phase=0;phase<2;phase++) {
        uint32_t push[4]={phase,0,0,0};
        vkCmdPushConstants(g->command,r->index.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        uint32_t count=phase?dc_gpu_body_capacity(g):((const uint32_t *)r->work.mapped)[8];
        vkCmdDispatch(g->command,(count+63u)/64u,1,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
}
uint32_t *dc_gpu_body_solver_state(const dc_gpu_t *g,uint32_t slot) {
    if(!g||slot>=dc_gpu_body_capacity(g)) return NULL;
    if(!g->runtime_body_consumers&&slot<64u)
        return dc_gpu_solver_data(g)+DC_GPU_SOLVER_HEADER_WORDS+slot*DC_GPU_SOLVER_BODY_WORDS;
    if(slot<64u) return (uint32_t *)g->runtime_bodies->initial_auxiliary.mapped+slot*64u;
    return dc_gpu_body_store_solver(&g->runtime_bodies->store,slot);
}
dc_body_storage_owner_t *dc_gpu_body_owner(const dc_gpu_t *g,uint32_t slot) {
    if(!g||!g->runtime_bodies||slot>=dc_gpu_body_capacity(g)) return NULL;
    if(slot<64u) return (dc_body_storage_owner_t *)((char *)g->runtime_bodies->initial_auxiliary.mapped+64u*256u)+slot;
    return dc_gpu_body_store_owner(&g->runtime_bodies->store,slot);
}
uint32_t *dc_gpu_body_hydro_state(const dc_gpu_t *g,uint32_t slot) {
    if(!g||slot>=dc_gpu_body_capacity(g)) return NULL;
    if(!g->runtime_body_consumers) return (uint32_t *)g->mpm_label_b_mapped+8192u+slot*128u;
    if(slot<64u) return (uint32_t *)g->runtime_bodies->initial_auxiliary.mapped+64u*(256u+32u)/4u+slot*128u;
    return dc_gpu_body_store_auxiliary(&g->runtime_bodies->store,slot);
}
void dc_gpu_runtime_record_legacy_state(dc_gpu_t *g) {
    if(!g||!g->runtime_body_consumers) return;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    uint32_t push[4]={2u,dc_gpu_solver_word_offset(g)+DC_GPU_SOLVER_HEADER_WORDS,
        dc_gpu_structural_word_offset(g)+dc_gpu_structural_graph_offset(),0};
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline_layout,0,1,&r->index.descriptor,0,NULL);
    vkCmdPushConstants(g->command,r->index.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,64u,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_HOST_READ_BIT);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
}
bool dc_gpu_runtime_prepare_fragments(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->runtime_body_consumers) return true;
    if(!dc_gpu_runtime_bodies_service(g,err,cap)||!fr_renderer_begin(g)) return false;
    struct dc_runtime_bodies *r=g->runtime_bodies;uint32_t push[4]={3u,0,0,0};
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline_layout,0,1,&r->index.descriptor,0,NULL);
    vkCmdPushConstants(g->command,r->index.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,(dc_gpu_body_capacity(g)+63u)/64u,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g)&&dc_gpu_runtime_bodies_service(g,err,cap);
}
bool dc_gpu_runtime_carve_retry(dc_gpu_t *g) {
    if(!g||!g->runtime_body_consumers) return false;
    uint32_t *mail=g->runtime_bodies->mailbox.mapped;
    bool retry=mail[10]!=0u;mail[10]=0u;return retry;
}
VkBuffer dc_gpu_runtime_body_work_buffer(const dc_gpu_t *g) {
    return g&&g->runtime_bodies?g->runtime_bodies->work.buffer:VK_NULL_HANDLE;
}
void dc_gpu_runtime_record_prefix(dc_gpu_t *g,uint32_t count,uint32_t indirect_word) {
    struct dc_runtime_bodies *r=g->runtime_bodies;
    const uint32_t *header=r->work.mapped;
    uint32_t source=header[9],destination=header[10];
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->scan.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->scan.pipeline_layout,0,1,&r->scan.descriptor,0,NULL);
    for(uint32_t span=1u;span<count;span*=2u) {
        uint32_t push[4]={span,source,destination,count};
        vkCmdPushConstants(g->command,r->scan.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        if(indirect_word!=UINT32_MAX) vkCmdDispatchIndirect(g->command,r->work.buffer,indirect_word*4u);
        else vkCmdDispatch(g->command,(count+127u)/128u,1,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        uint32_t previous=source;source=destination;destination=previous;
        if(span>UINT32_MAX/2u) break;
    }
    if(source!=header[9]) {
        uint32_t push[4]={0,source,header[9],count};
        vkCmdPushConstants(g->command,r->scan.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        if(indirect_word!=UINT32_MAX) vkCmdDispatchIndirect(g->command,r->work.buffer,indirect_word*4u);
        else vkCmdDispatch(g->command,(count+127u)/128u,1,1);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    }
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
}
