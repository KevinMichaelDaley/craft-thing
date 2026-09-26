#include <stdio.h>
#include <string.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(gpu->device, &info, NULL, buffer) != VK_SUCCESS)
        return error(err, cap, "Cannot create GPU chunk buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu->device, *buffer, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &props);
    uint32_t type = UINT32_MAX;
    VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((req.memoryTypeBits & (1u << i)) &&
            (props.memoryTypes[i].propertyFlags & needed) == needed) { type = i; break; }
    }
    if (type == UINT32_MAX) return error(err, cap, "No host-coherent GPU chunk memory");
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    if (vkAllocateMemory(gpu->device, &alloc, NULL, memory) != VK_SUCCESS ||
        vkBindBufferMemory(gpu->device, *buffer, *memory, 0) != VK_SUCCESS ||
        vkMapMemory(gpu->device, *memory, 0, bytes, 0, mapped) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate or map GPU chunk memory");
    memset(*mapped, 0, (size_t)bytes);
    return true;
}

bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    gpu->page_width = (gpu->width + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    gpu->page_height = (gpu->height + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    VkDeviceSize chunk_bytes = (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize page_bytes = (VkDeviceSize)gpu->page_width * gpu->page_height * sizeof(uint32_t);
    return dc_gpu_make_mapped_buffer(gpu, chunk_bytes, &gpu->chunk_buffer,
               &gpu->chunk_memory, &gpu->chunk_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, page_bytes, &gpu->page_buffer,
               &gpu->page_memory, &gpu->page_mapped, err, cap) &&
           dc_gpu_halo_buffers_init(gpu, err, cap) &&
           dc_gpu_fluid_buffers_init(gpu, err, cap);
}

void dc_gpu_chunks_destroy(dc_gpu_t *gpu) {
    dc_gpu_halo_destroy(gpu);
    dc_gpu_fluid_destroy(gpu);
    if (gpu->chunk_mapped) vkUnmapMemory(gpu->device, gpu->chunk_memory);
    if (gpu->page_mapped) vkUnmapMemory(gpu->device, gpu->page_memory);
    if (gpu->chunk_buffer) vkDestroyBuffer(gpu->device, gpu->chunk_buffer, NULL);
    if (gpu->page_buffer) vkDestroyBuffer(gpu->device, gpu->page_buffer, NULL);
    if (gpu->chunk_memory) vkFreeMemory(gpu->device, gpu->chunk_memory, NULL);
    if (gpu->page_memory) vkFreeMemory(gpu->device, gpu->page_memory, NULL);
}

bool dc_gpu_upload_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                         char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= DC_GPU_CHUNK_SLOTS)
        return error(err, cap, "Invalid GPU chunk upload slot");
    dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(cells + (size_t)slot * DC_CHUNK_CELLS, chunk->cells, sizeof(chunk->cells));
    return true;
}

bool dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                           char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= DC_GPU_CHUNK_SLOTS)
        return error(err, cap, "Invalid GPU chunk download slot");
    const dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(chunk->cells, cells + (size_t)slot * DC_CHUNK_CELLS, sizeof(chunk->cells));
    return true;
}

bool dc_gpu_set_page(dc_gpu_t *gpu, uint32_t tile_x, uint32_t tile_y,
                     uint32_t slot, char *err, uint32_t cap) {
    if (!gpu || tile_x >= gpu->page_width || tile_y >= gpu->page_height ||
        (slot != UINT32_MAX && slot >= DC_GPU_CHUNK_SLOTS))
        return error(err, cap, "Invalid GPU page mapping");
    uint32_t *pages = gpu->page_mapped;
    uint32_t tile = tile_y * gpu->page_width + tile_x;
    if (slot != UINT32_MAX && gpu->slot_page[slot] != UINT32_MAX &&
        gpu->slot_page[slot] != tile)
        pages[gpu->slot_page[slot]] = 0u;
    uint32_t old_page = pages[tile];
    if (old_page && (slot == UINT32_MAX || old_page != slot + 1u))
        gpu->slot_page[old_page - 1u] = UINT32_MAX;
    pages[tile] = slot == UINT32_MAX ? 0u : slot + 1u;
    if (slot != UINT32_MAX) gpu->slot_page[slot] = tile;
    return true;
}

bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err, uint32_t cap) {
    if (!gpu || !cells || cell_count < (uint64_t)gpu->width * gpu->height)
        return error(err, cap, "Readback buffer is too small");
    memcpy(cells, gpu->mapped, (size_t)gpu->width * gpu->height * sizeof(uint32_t));
    return true;
}
