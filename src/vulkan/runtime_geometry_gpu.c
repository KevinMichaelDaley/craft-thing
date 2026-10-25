#include <stdio.h>
#include <string.h>
#include "runtime_body_internal.h"
#include "reactive_gpu_internal.h"

static dc_body_address_t address(VkDeviceAddress a) {
    return (dc_body_address_t){(uint32_t)a,(uint32_t)(a>>32)};
}
static void copy(VkCommandBuffer command,VkBuffer source,VkBuffer target,
                 VkDeviceSize from,VkDeviceSize to,VkDeviceSize size) {
    VkBufferCopy region={from,to,size};vkCmdCopyBuffer(command,source,target,1,&region);
}
static bool storage_reserve(dc_gpu_t *g,uint32_t required,uint32_t pairs,uint32_t contacts,char *err,uint32_t cap) {
    uint32_t old=dc_gpu_pixel_pool_capacity(g),next=old;
    uint32_t old_pairs=g->runtime_pair_capacity?g->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY;
    uint32_t old_contacts=g->runtime_contact_capacity?g->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY;
    if(required<=old&&pairs<=old_pairs&&contacts<=old_contacts) return true;
    if(pairs<old_pairs) pairs=old_pairs;
    if(contacts<old_contacts) contacts=old_contacts;
    dc_gpu_t layout=*g;layout.runtime_pair_capacity=pairs;layout.runtime_contact_capacity=contacts;
    while(next<required) {
        if(next>UINT32_MAX/256u) {
            if(err&&cap) snprintf(err,cap,"Pixel appearance address range exhausted");
            return false;
        }
        next*=2u;
    }
    layout.pixel_pool_words=next;
    if(!fr_gpu_reactive_reserve_shapes(g,next,err,cap)) return false;
    struct dc_runtime_bodies *r=g->runtime_bodies;
    VkDeviceSize old_prefix=(VkDeviceSize)dc_gpu_pixel_pool_word_offset(g)*4u+64u;
    VkDeviceSize prefix=(VkDeviceSize)dc_gpu_pixel_pool_word_offset(&layout)*4u+64u;
    VkDeviceSize body_prefix=64u*sizeof(dc_gpu_body_record_t);
    VkDeviceSize old_contact=body_prefix+dc_gpu_contact_word_offset(g)*4u;
    VkDeviceSize new_contact=body_prefix+dc_gpu_contact_word_offset(&layout)*4u;
    VkDeviceSize old_solver=body_prefix+dc_gpu_solver_word_offset(g)*4u;
    VkDeviceSize new_solver=body_prefix+dc_gpu_solver_word_offset(&layout)*4u;
    VkDeviceSize old_structure=body_prefix+dc_gpu_structural_word_offset(g)*4u;
    VkDeviceSize new_structure=body_prefix+dc_gpu_structural_word_offset(&layout)*4u;
    VkDeviceSize terrain=dc_gpu_color_terrain_bytes(),old_bank=(VkDeviceSize)old*128u,new_bank=(VkDeviceSize)next*128u;
    dc_gpu_address_buffer_t metadata={0},colors={0};
    if(!dc_gpu_address_buffer_create(g,prefix+(VkDeviceSize)next*8u+dc_gpu_contact_work_bytes(&layout),true,&metadata,err,cap)||
       !dc_gpu_address_buffer_create(g,terrain+new_bank*2u,true,&colors,err,cap)) {
        dc_gpu_address_buffer_destroy(g,&metadata);return false;
    }
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) goto fail;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    dc_body_storage_root_t previous=*(dc_body_storage_root_t *)r->root.mapped;
    dc_body_address_t records=address(metadata.address);
    vkCmdUpdateBuffer(g->command,r->store.directory.buffer,0,sizeof(records),&records);
    dc_body_storage_root_t *root=r->root.mapped;
    root->data=address(metadata.address+64u*sizeof(dc_gpu_body_record_t));root->geometry=address(metadata.address);
    copy(g->command,r->metadata.buffer,metadata.buffer,0,0,old_contact);
    copy(g->command,r->metadata.buffer,metadata.buffer,old_contact,new_contact,old_solver-old_contact);
    copy(g->command,r->metadata.buffer,metadata.buffer,old_solver,new_solver,old_structure-old_solver);
    copy(g->command,r->metadata.buffer,metadata.buffer,old_structure,new_structure,old_prefix-old_structure);
    copy(g->command,r->metadata.buffer,metadata.buffer,old_prefix,prefix,(VkDeviceSize)old*4u);
    copy(g->command,r->metadata.buffer,metadata.buffer,old_prefix+(VkDeviceSize)old*4u,prefix+(VkDeviceSize)next*4u,(VkDeviceSize)old*4u);
    copy(g->command,g->shape_color_buffer,colors.buffer,0,0,terrain+old_bank);
    copy(g->command,g->shape_color_buffer,colors.buffer,terrain+old_bank,terrain+new_bank,old_bank);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    vkCmdUpdateBuffer(g->command,metadata.buffer,prefix-64u+8u,4,&next);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t previous_capacity=((uint32_t *)r->work.mapped)[29];
    ((uint32_t *)r->work.mapped)[29]=next;
    uint32_t push[4]={4u,(uint32_t)(old_prefix/4u),old,(uint32_t)(prefix/4u)};
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->index.pipeline_layout,0,1,&r->index.descriptor,0,NULL);
    vkCmdPushConstants(g->command,r->index.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,(dc_gpu_body_capacity(g)+63u)/64u,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) {
        *root=previous;
        ((uint32_t *)r->work.mapped)[29]=previous_capacity;goto fail;
    }
    dc_gpu_address_buffer_t old_colors={.buffer=g->shape_color_buffer,.memory=g->shape_color_memory,
        .mapped=g->shape_color_mapped,.address=g->shape_color_address,.bytes=terrain+old_bank*2u,
        .allocation_bytes=g->shape_color_allocation_bytes,.host_visible=true,.device_local=g->shape_color_device_local};
    dc_gpu_address_buffer_destroy(g,&r->metadata);r->metadata=metadata;
    g->body_buffer=metadata.buffer;g->body_memory=metadata.memory;g->body_mapped=metadata.mapped;
    g->shape_color_buffer=colors.buffer;g->shape_color_memory=colors.memory;g->shape_color_mapped=colors.mapped;
    g->shape_color_address=colors.address;g->shape_color_allocation_bytes=colors.allocation_bytes;
    g->shape_color_device_local=colors.device_local;g->pixel_pool_words=next;
    g->runtime_pair_capacity=pairs;g->runtime_contact_capacity=contacts;
    uint32_t *bp=dc_gpu_broadphase_data(g),*ct=dc_gpu_contact_data(g);
    if(bp[3]==old_pairs) bp[3]=pairs;
    if(ct[3]==old_contacts) ct[3]=contacts;
    uint32_t *h=r->work.mapped;h[29]=next;h[36]=pairs;h[37]=contacts;
    dc_body_address_t a=address(colors.address+terrain),b=address(colors.address+terrain+new_bank);
    h[32]=a.low;h[33]=a.high;h[34]=b.low;h[35]=b.high;
    dc_body_address_t work=address(metadata.address+prefix+(VkDeviceSize)next*8u);
    h[45]=work.low;h[46]=work.high;
    VkDescriptorBufferInfo binding[3]={{colors.buffer,0,terrain},{colors.buffer,0,16},{colors.buffer,0,16}};
    VkWriteDescriptorSet write[3]={0};
    for(uint32_t i=0;i<3;i++) write[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=g->descriptor,
        .dstBinding=33+i,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.pBufferInfo=binding+i};
    vkUpdateDescriptorSets(g->device,3,write,0,NULL);
    g->descriptor_revision++;fr_renderer_rebind_colors(g);
    dc_gpu_address_buffer_destroy(g,&old_colors);return true;
fail:
    dc_gpu_address_buffer_destroy(g,&metadata);dc_gpu_address_buffer_destroy(g,&colors);
    if(err&&cap) snprintf(err,cap,"Cannot complete pixel geometry growth");
    return false;
}

bool dc_gpu_runtime_geometry_reserve(dc_gpu_t *g,uint32_t words,char *err,uint32_t cap) {
    return storage_reserve(g,words,0,0,err,cap);
}
bool dc_gpu_runtime_queues_reserve(dc_gpu_t *g,uint32_t pairs,uint32_t contacts,char *err,uint32_t cap) {
    uint32_t p=g->runtime_pair_capacity?g->runtime_pair_capacity:DC_GPU_BROADPHASE_PAIR_CAPACITY;
    uint32_t c=g->runtime_contact_capacity?g->runtime_contact_capacity:DC_GPU_CONTACT_CAPACITY;
    while(p<pairs) {if(p>UINT32_MAX/256u) return false;p*=2u;}
    while(c<contacts) {if(c>UINT32_MAX/256u) return false;c*=2u;}
    return storage_reserve(g,dc_gpu_pixel_pool_capacity(g),p,c,err,cap);
}
