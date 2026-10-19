#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_internal.h"
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
    if(bank>properties.limits.maxStorageBufferRange || terrain>properties.limits.maxStorageBufferRange) {
        snprintf(err,cap,"Pixel appearance exceeds device storage-buffer range");return false;
    }
    if(!dc_gpu_make_mapped_buffer(g,terrain+2*bank,&g->shape_color_buffer,
        &g->shape_color_memory,&g->shape_color_mapped,err,cap)) return false;
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
bool fr_renderer_upload_chunk_colors(dc_gpu_t *g,uint32_t slot,const dc_chunk_colors_t *colors,char *err,uint32_t cap) {
    if(!g||!colors||slot>=DC_GPU_CHUNK_SLOTS||
        !dc_color_records_valid((const float*)colors,DC_CHUNK_CELLS*2)||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) {
        if(err&&cap) snprintf(err,cap,"Invalid saved color map");
        return false;
    }
    dc_gpu_write_chunk_colors(g,slot,colors);return true;
}
