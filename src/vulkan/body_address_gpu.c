#include <stdio.h>
#include <string.h>

#include "body_address_gpu.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void release_partial(dc_gpu_t *gpu,dc_gpu_address_buffer_t *buffer) {
    if(buffer->mapped) vkUnmapMemory(gpu->device,buffer->memory);
    if(buffer->buffer) vkDestroyBuffer(gpu->device,buffer->buffer,NULL);
    if(buffer->memory) vkFreeMemory(gpu->device,buffer->memory,NULL);
}
bool dc_gpu_address_buffer_create(dc_gpu_t *gpu,VkDeviceSize bytes,bool host_visible,
    dc_gpu_address_buffer_t *out,char *err,uint32_t cap) {
    if(!gpu||!out||!bytes||out->buffer||out->memory) return error(err,cap,"Invalid address buffer destination or size");
    dc_gpu_address_buffer_t pending={.bytes=bytes,.host_visible=host_visible};
    VkBufferCreateInfo info={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=bytes,
        .usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT|
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE};
    if(vkCreateBuffer(gpu->device,&info,NULL,&pending.buffer)!=VK_SUCCESS)
        return error(err,cap,"Cannot create addressable GPU buffer");
    VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(gpu->device,pending.buffer,&requirements);
    VkPhysicalDeviceMemoryProperties properties;vkGetPhysicalDeviceMemoryProperties(gpu->physical,&properties);
    VkMemoryPropertyFlags needed=host_visible?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryAllocateFlagsInfo flags={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
    VkMemoryAllocateInfo allocation={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.pNext=&flags,.allocationSize=requirements.size};
    for(uint32_t type=0;type<properties.memoryTypeCount;type++) {
        VkMemoryPropertyFlags available=properties.memoryTypes[type].propertyFlags;
        if((requirements.memoryTypeBits&(1u<<type))==0u||(available&needed)!=needed) continue;
        VkDeviceMemory memory=VK_NULL_HANDLE;allocation.memoryTypeIndex=type;
        if(vkAllocateMemory(gpu->device,&allocation,NULL,&memory)!=VK_SUCCESS) continue;
        pending.memory=memory;pending.device_local=(available&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)!=0;
        break;
    }
    if(!pending.memory) {release_partial(gpu,&pending);return error(err,cap,"Cannot allocate addressable GPU memory");}
    if(vkBindBufferMemory(gpu->device,pending.buffer,pending.memory,0)!=VK_SUCCESS) {
        release_partial(gpu,&pending);return error(err,cap,"Cannot bind addressable GPU memory");
    }
    if(host_visible&&vkMapMemory(gpu->device,pending.memory,0,bytes,0,&pending.mapped)!=VK_SUCCESS) {
        release_partial(gpu,&pending);return error(err,cap,"Cannot map addressable GPU memory");
    }
    VkBufferDeviceAddressInfo address={.sType=VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer=pending.buffer};
    pending.address=vkGetBufferDeviceAddress(gpu->device,&address);
    if(!pending.address) {release_partial(gpu,&pending);return error(err,cap,"Addressable GPU buffer has no device address");}
    pending.allocation_bytes=requirements.size;
    if(host_visible) {
        memset(pending.mapped,0,(size_t)bytes);
        if(pending.device_local) gpu->mapped_local_bytes+=requirements.size;
        else gpu->mapped_system_bytes+=requirements.size;
    } else gpu->device_only_bytes+=requirements.size;
    *out=pending;return true;
}
void dc_gpu_address_buffer_destroy(dc_gpu_t *gpu,dc_gpu_address_buffer_t *buffer) {
    if(!gpu||!buffer||!buffer->buffer) return;
    release_partial(gpu,buffer);
    if(buffer->host_visible) {
        if(buffer->device_local) gpu->mapped_local_bytes-=buffer->allocation_bytes;
        else gpu->mapped_system_bytes-=buffer->allocation_bytes;
    } else gpu->device_only_bytes-=buffer->allocation_bytes;
    *buffer=(dc_gpu_address_buffer_t){0};
}
