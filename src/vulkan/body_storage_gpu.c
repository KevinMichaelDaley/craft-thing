#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "body_storage_gpu.h"

struct dc_gpu_body_block {
    dc_gpu_address_buffer_t storage;
    dc_gpu_body_block_t *next;
};
enum {SOLVER_BYTES=32u*sizeof(uint32_t),OWNERSHIP_BYTES=sizeof(dc_body_storage_owner_t),AUXILIARY_BYTES=32u};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static dc_body_address_t address(VkDeviceAddress value) {
    return (dc_body_address_t){(uint32_t)value,(uint32_t)(value>>32)};
}
static void discard_resources(dc_gpu_t *gpu,dc_gpu_body_growth_t *growth) {
    dc_gpu_address_buffer_destroy(gpu,&growth->directory);
    dc_gpu_address_buffer_destroy(gpu,&growth->upload);
    if(growth->block) {
        dc_gpu_address_buffer_destroy(gpu,&growth->block->storage);
        free(growth->block);
    }
    *growth=(dc_gpu_body_growth_t){0};
}
bool dc_gpu_body_growth_prepare(dc_gpu_t *gpu,const dc_gpu_body_store_t *store,
    uint32_t required_slots,dc_gpu_body_growth_t *growth,char *err,uint32_t cap) {
    if(!gpu||!store||!growth||growth->block||growth->directory.buffer||growth->upload.buffer||
       growth->submission||!required_slots||required_slots==DC_BODY_SLOT_NONE)
        return error(err,cap,"Invalid body growth preparation");
    uint32_t slabs=(required_slots-1u)/DC_BODY_SLAB_RECORDS+1u;
    if(slabs<=store->slab_count) return error(err,cap,"Body storage already satisfies request");
    dc_gpu_body_growth_t pending={.previous_slabs=store->slab_count,.slab_count=slabs};
    pending.block=calloc(1,sizeof(*pending.block));
    if(!pending.block) return error(err,cap,"Cannot allocate body resource ownership metadata");
    VkDeviceSize added=slabs-store->slab_count,slots=added*DC_BODY_SLAB_RECORDS;
    VkDeviceSize record_bytes=slots*sizeof(dc_gpu_body_record_t);
    VkDeviceSize solver_bytes=slots*SOLVER_BYTES,ownership_bytes=slots*OWNERSHIP_BYTES;
    VkDeviceSize bytes=record_bytes+solver_bytes+ownership_bytes+slots*AUXILIARY_BYTES;
    if(!dc_gpu_address_buffer_create(gpu,bytes,false,&pending.block->storage,err,cap)||
       !dc_gpu_address_buffer_create(gpu,(VkDeviceSize)slabs*sizeof(dc_body_storage_page_t),false,&pending.directory,err,cap)||
       !dc_gpu_address_buffer_create(gpu,added*sizeof(dc_body_storage_page_t),true,&pending.upload,err,cap)) {
        discard_resources(gpu,&pending);return false;
    }
    dc_body_storage_page_t *pages=pending.upload.mapped;
    VkDeviceAddress base=pending.block->storage.address;
    for(uint32_t page=0;page<added;page++) {
        VkDeviceSize slot=(VkDeviceSize)page*DC_BODY_SLAB_RECORDS;
        pages[page]=(dc_body_storage_page_t){
            .records=address(base+slot*sizeof(dc_gpu_body_record_t)),
            .solver=address(base+record_bytes+slot*SOLVER_BYTES),
            .ownership=address(base+record_bytes+solver_bytes+slot*OWNERSHIP_BYTES),
            .auxiliary=address(base+record_bytes+solver_bytes+ownership_bytes+slot*AUXILIARY_BYTES),
            .free_mask={UINT32_MAX,UINT32_MAX},.generation=1u};
    }
    *growth=pending;return true;
}
static void barrier(VkCommandBuffer command,VkPipelineStageFlags2 source,VkAccessFlags2 access,
    VkPipelineStageFlags2 destination,VkAccessFlags2 destination_access) {
    VkMemoryBarrier2 memory={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask=source,.srcAccessMask=access,.dstStageMask=destination,.dstAccessMask=destination_access};
    VkDependencyInfo info={.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.memoryBarrierCount=1,.pMemoryBarriers=&memory};
    vkCmdPipelineBarrier2(command,&info);
}
bool dc_gpu_body_growth_record(VkCommandBuffer command,VkBuffer root,
    const dc_gpu_body_store_t *store,dc_gpu_body_growth_t *growth,uint64_t submission) {
    if(!command||!root||!store||!growth||!growth->block||!submission||growth->submission||
       growth->previous_slabs!=store->slab_count||growth->slab_count<=store->slab_count) return false;
    barrier(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    vkCmdFillBuffer(command,growth->block->storage.buffer,0,VK_WHOLE_SIZE,0);
    if(store->slab_count) {
        VkBufferCopy copy={.size=(VkDeviceSize)store->slab_count*sizeof(dc_body_storage_page_t)};
        vkCmdCopyBuffer(command,store->directory.buffer,growth->directory.buffer,1,&copy);
    }
    VkBufferCopy copy={.dstOffset=(VkDeviceSize)store->slab_count*sizeof(dc_body_storage_page_t),.size=growth->upload.bytes};
    vkCmdCopyBuffer(command,growth->upload.buffer,growth->directory.buffer,1,&copy);
    barrier(command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    dc_body_address_t directory=address(growth->directory.address);
    vkCmdUpdateBuffer(command,root,offsetof(dc_body_storage_root_t,directory),sizeof(directory),&directory);
    uint32_t counts[]={growth->slab_count,growth->slab_count};
    vkCmdUpdateBuffer(command,root,offsetof(dc_body_storage_root_t,slab_count),sizeof(counts),counts);
    if(!store->slab_count) {
        uint32_t initial[]={0,DC_BODY_SLAB_SHIFT,DC_BODY_STORAGE_ABI,1};
        vkCmdUpdateBuffer(command,root,offsetof(dc_body_storage_root_t,live_count),sizeof(initial),initial);
    }
    barrier(command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);
    growth->submission=submission;return true;
}
bool dc_gpu_body_growth_finish(dc_gpu_t *gpu,dc_gpu_body_store_t *store,
    dc_gpu_body_growth_t *growth,uint64_t completed_submission) {
    if(!gpu||!store||!growth||!growth->block||!growth->submission||
       completed_submission<growth->submission||store->slab_count!=growth->previous_slabs) return false;
    dc_gpu_address_buffer_destroy(gpu,&store->directory);
    dc_gpu_address_buffer_destroy(gpu,&growth->upload);
    growth->block->next=store->blocks;store->blocks=growth->block;
    store->directory=growth->directory;store->slab_count=growth->slab_count;
    *growth=(dc_gpu_body_growth_t){0};return true;
}
bool dc_gpu_body_growth_discard(dc_gpu_t *gpu,dc_gpu_body_growth_t *growth) {
    if(!gpu||!growth||growth->submission) return false;
    discard_resources(gpu,growth);return true;
}
void dc_gpu_body_store_destroy(dc_gpu_t *gpu,dc_gpu_body_store_t *store) {
    if(!gpu||!store) return;
    dc_gpu_address_buffer_destroy(gpu,&store->directory);
    while(store->blocks) {
        dc_gpu_body_block_t *block=store->blocks;store->blocks=block->next;
        dc_gpu_address_buffer_destroy(gpu,&block->storage);free(block);
    }
    *store=(dc_gpu_body_store_t){0};
}
