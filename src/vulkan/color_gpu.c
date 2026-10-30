#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_internal.h"
#include "body_address_gpu.h"
#include "renderer_internal.h"
#include "ferrum/renderer/gpu.h"

VkDeviceSize dc_gpu_color_terrain_bytes(void) {
    return (VkDeviceSize)DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS*16;
}
VkDeviceSize dc_gpu_color_bank_bytes(const dc_gpu_t *g) {
    return (VkDeviceSize)dc_gpu_pixel_pool_capacity(g)*8*16;
}
bool dc_gpu_colors_init(dc_gpu_t *g,char *err,uint32_t cap) {
    VkDeviceSize terrain=dc_gpu_color_terrain_bytes(),bank=dc_gpu_color_bank_bytes(g);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    if(terrain>properties.limits.maxStorageBufferRange) {
        snprintf(err,cap,"Pixel appearance exceeds device storage-buffer range");return false;
    }
    dc_gpu_address_buffer_t colors={0};
    if(!dc_gpu_address_buffer_create(g,terrain+2*bank,true,&colors,err,cap)) return false;
    g->shape_color_buffer=colors.buffer;g->shape_color_memory=colors.memory;
    g->shape_color_mapped=colors.mapped;g->shape_color_address=colors.address;
    g->shape_color_allocation_bytes=colors.allocation_bytes;
    g->shape_color_device_local=colors.device_local;
    memset(g->shape_color_mapped,0,(size_t)(terrain+2*bank));return true;
}
void dc_gpu_colors_destroy(dc_gpu_t *g) {
    free(g->checkpoint_colors);
    if(g->shape_color_mapped) vkUnmapMemory(g->device,g->shape_color_memory);
    if(g->shape_color_buffer) vkDestroyBuffer(g->device,g->shape_color_buffer,NULL);
    if(g->shape_color_memory) vkFreeMemory(g->device,g->shape_color_memory,NULL);
}
float *dc_gpu_shape_colors(dc_gpu_t *g,uint32_t word) {
    size_t relative=word-dc_gpu_pixel_pool_word_offset(g)-DC_GPU_PIXEL_POOL_HEADER_WORDS;
    return (float*)((uint8_t*)g->shape_color_mapped+dc_gpu_color_terrain_bytes())+relative*32;
}
void dc_gpu_read_chunk_colors(dc_gpu_t *g,uint32_t slot,dc_chunk_colors_t *colors) {
    memcpy(colors->terrain,(float*)g->shape_color_mapped+(size_t)slot*4096*4,sizeof(colors->terrain));
    memcpy(colors->water,(float*)g->water_color_mapped+(size_t)slot*4096*4,sizeof(colors->water));
}
void dc_gpu_write_chunk_colors(dc_gpu_t *g,uint32_t slot,const dc_chunk_colors_t *colors) {
    memcpy((float*)g->shape_color_mapped+(size_t)slot*4096*4,colors->terrain,sizeof(colors->terrain));
    memcpy((float*)g->water_color_mapped+(size_t)slot*4096*4,colors->water,sizeof(colors->water));
}
bool fr_renderer_download_chunk_colors(dc_gpu_t *g,uint32_t slot,dc_chunk_colors_t *colors,char *err,uint32_t cap) {
    if(!g||!colors||slot>=DC_GPU_CHUNK_SLOTS||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) {
        if(err&&cap) snprintf(err,cap,"Invalid color download boundary");
        return false;
    }
    dc_gpu_read_chunk_colors(g,slot,colors);return true;
}
bool fr_renderer_capture_chunk_colors(dc_gpu_t *g,dc_chunk_colors_t *colors,
                                      uint32_t slots,char *err,uint32_t cap) {
    if(!g||!colors||slots==0u||slots>DC_GPU_CHUNK_SLOTS) {
        if(err&&cap) snprintf(err,cap,"Invalid color snapshot boundary");
        return false;
    }
    VkDeviceSize bytes=dc_gpu_color_terrain_bytes();
    VkBuffer staging=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void *mapped=NULL;
    VkDeviceSize local_before=g->mapped_local_bytes,system_before=g->mapped_system_bytes;
    bool okay=dc_gpu_make_staging_buffer(g,bytes,&staging,&memory,&mapped,err,cap);
    VkDeviceSize local_added=g->mapped_local_bytes-local_before;
    VkDeviceSize system_added=g->mapped_system_bytes-system_before;
    if(okay) okay=vkQueueWaitIdle(g->queue)==VK_SUCCESS&&fr_renderer_begin(g);
    if(okay) {
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferCopy region={0,0,bytes};
        vkCmdCopyBuffer(g->command,g->shape_color_buffer,staging,1,&region);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
        okay=fr_renderer_submit(g);
    }
    if(okay) for(uint32_t i=0;i<slots;i++) {
        memcpy(colors[i].terrain,(const uint8_t *)mapped+(size_t)i*sizeof(colors[i].terrain),
            sizeof(colors[i].terrain));
        memcpy(colors[i].water,(const uint8_t *)g->water_color_mapped+(size_t)i*sizeof(colors[i].water),
            sizeof(colors[i].water));
    }
    if(mapped) vkUnmapMemory(g->device,memory);
    if(staging) vkDestroyBuffer(g->device,staging,NULL);
    if(memory) vkFreeMemory(g->device,memory,NULL);
    g->mapped_local_bytes-=local_added;
    g->mapped_system_bytes-=system_added;
    if(!okay&&err&&cap) snprintf(err,cap,"Cannot capture chunk colors for save");
    return okay;
}
bool fr_renderer_upload_chunk_colors(dc_gpu_t *g,uint32_t slot,const dc_chunk_colors_t *colors,char *err,uint32_t cap) {
    if(!g||!colors||slot>=DC_GPU_CHUNK_SLOTS||
        !dc_color_records_valid((const float*)colors,DC_CHUNK_CELLS*2)||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) {
        if(err&&cap) snprintf(err,cap,"Invalid saved color map");
        return false;
    }
    dc_gpu_write_chunk_colors(g,slot,colors);return true;
}
