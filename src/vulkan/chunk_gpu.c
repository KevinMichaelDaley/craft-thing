#include <stdio.h>
#include <stdlib.h>
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
    gpu->chunk_velocity = calloc((size_t)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS,
                                  sizeof(*gpu->chunk_velocity));
    if (!gpu->chunk_velocity)
        return error(err, cap, "Cannot allocate streamed chunk velocity staging");
    gpu->page_width = (gpu->width + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    gpu->page_height = (gpu->height + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    VkDeviceSize chunk_bytes = (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize page_bytes = (VkDeviceSize)gpu->page_width * gpu->page_height * sizeof(uint32_t);
    return dc_gpu_make_mapped_buffer(gpu, chunk_bytes, &gpu->chunk_buffer,
               &gpu->chunk_memory, &gpu->chunk_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu,
               (VkDeviceSize)DC_GPU_CHUNK_SLOTS * DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t),
               &gpu->particle_buffer, &gpu->particle_memory, &gpu->particle_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, DC_GPU_CHUNK_SLOTS * sizeof(uint32_t),
               &gpu->particle_count_buffer, &gpu->particle_count_memory,
               &gpu->particle_count_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, page_bytes, &gpu->page_buffer,
               &gpu->page_memory, &gpu->page_mapped, err, cap) &&
           dc_gpu_halo_buffers_init(gpu, err, cap) &&
           dc_gpu_fluid_buffers_init(gpu, err, cap) &&
           dc_gpu_marker_buffers_init(gpu, err, cap);
}

void dc_gpu_chunks_destroy(dc_gpu_t *gpu) {
    free(gpu->chunk_velocity);
    dc_gpu_halo_destroy(gpu);
    dc_gpu_fluid_destroy(gpu);
    dc_gpu_marker_destroy(gpu);
    if (gpu->chunk_mapped) vkUnmapMemory(gpu->device, gpu->chunk_memory);
    if (gpu->particle_mapped) vkUnmapMemory(gpu->device, gpu->particle_memory);
    if (gpu->particle_count_mapped) vkUnmapMemory(gpu->device, gpu->particle_count_memory);
    if (gpu->page_mapped) vkUnmapMemory(gpu->device, gpu->page_memory);
    if (gpu->chunk_buffer) vkDestroyBuffer(gpu->device, gpu->chunk_buffer, NULL);
    if (gpu->particle_buffer) vkDestroyBuffer(gpu->device, gpu->particle_buffer, NULL);
    if (gpu->particle_count_buffer) vkDestroyBuffer(gpu->device, gpu->particle_count_buffer, NULL);
    if (gpu->page_buffer) vkDestroyBuffer(gpu->device, gpu->page_buffer, NULL);
    if (gpu->chunk_memory) vkFreeMemory(gpu->device, gpu->chunk_memory, NULL);
    if (gpu->particle_memory) vkFreeMemory(gpu->device, gpu->particle_memory, NULL);
    if (gpu->particle_count_memory) vkFreeMemory(gpu->device, gpu->particle_count_memory, NULL);
    if (gpu->page_memory) vkFreeMemory(gpu->device, gpu->page_memory, NULL);
}

bool dc_gpu_upload_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                         char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= DC_GPU_CHUNK_SLOTS ||
        chunk->marker_count > DC_MARKERS_PER_CHUNK ||
        chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK)
        return error(err, cap, "Invalid GPU chunk upload slot");
    uint32_t particle_count = 0, missing_primary = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        particle_count += chunk->particles[i].mass_fp != 0u;
    if (particle_count != chunk->particle_count)
        return error(err, cap, "Chunk particle count does not match active records");
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t material = chunk->cells[i].material;
        if ((material == DC_MATERIAL_SAND || material == DC_MATERIAL_DIRT ||
             material == DC_MATERIAL_GRAVEL) && chunk->particles[i].mass_fp == 0u)
            ++missing_primary;
    }
    if (missing_primary > DC_MPM_PARTICLES_PER_CHUNK - particle_count)
        return error(err, cap, "GPU particle pool is full");
    dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(cells + (size_t)slot * DC_CHUNK_CELLS, chunk->cells, sizeof(chunk->cells));
    memcpy(gpu->chunk_velocity + (size_t)slot * DC_CHUNK_CELLS,
           chunk->face_velocity, sizeof(chunk->face_velocity));
    gpu->preserve_shifted_slot[slot] = false;
    dc_marker_t *markers_a = gpu->marker_a_mapped;
    dc_marker_t *markers_b = gpu->marker_b_mapped;
    size_t offset = (size_t)slot * DC_MARKERS_PER_CHUNK;
    memcpy(markers_a + offset, chunk->markers,
           (size_t)chunk->marker_count * sizeof(dc_marker_t));
    memcpy(markers_b + offset, chunk->markers,
           (size_t)chunk->marker_count * sizeof(dc_marker_t));
    ((uint32_t *)gpu->marker_count_a_mapped)[slot] = chunk->marker_count;
    ((uint32_t *)gpu->marker_count_b_mapped)[slot] = chunk->marker_count;
    memset((uint32_t *)gpu->marker_grid_mapped + (size_t)slot * DC_CHUNK_CELLS,
           0, DC_CHUNK_CELLS * sizeof(uint32_t));
    dc_mpm_particle_t *particles = (dc_mpm_particle_t *)gpu->particle_mapped +
        (size_t)slot * DC_MPM_PARTICLES_PER_CHUNK;
    memcpy(particles, chunk->particles, sizeof(chunk->particles));
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t material = chunk->cells[i].material;
        if ((material == DC_MATERIAL_SAND || material == DC_MATERIAL_DIRT ||
             material == DC_MATERIAL_GRAVEL) && particles[i].mass_fp == 0u) {
            dc_chunk_particle_init(&particles[i], chunk->coord, i, material);
            ++particle_count;
        }
    }
    ((uint32_t *)gpu->particle_count_mapped)[slot] = particle_count;
    ((uint32_t *)gpu->slot_seed_mapped)[slot] = dc_chunk_particle_seed(chunk->coord);
    return true;
}

bool dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                           char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= DC_GPU_CHUNK_SLOTS)
        return error(err, cap, "Invalid GPU chunk download slot");
    const dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(chunk->cells, cells + (size_t)slot * DC_CHUNK_CELLS, sizeof(chunk->cells));
    uint32_t tile = gpu->slot_page[slot];
    if (tile != UINT32_MAX) {
        const dc_face_velocity_t *faces = gpu->velocity_mapped;
        uint32_t tile_x = tile % gpu->page_width;
        uint32_t tile_y = tile / gpu->page_width;
        for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
            memcpy(chunk->face_velocity + (size_t)y * DC_CHUNK_SIDE,
                   faces + ((size_t)(tile_y * DC_CHUNK_SIDE + y) * gpu->width +
                            tile_x * DC_CHUNK_SIDE),
                   DC_CHUNK_SIDE * sizeof(dc_face_velocity_t));
    } else {
        memcpy(chunk->face_velocity,
               gpu->chunk_velocity + (size_t)slot * DC_CHUNK_CELLS,
               sizeof(chunk->face_velocity));
    }
    const uint32_t *counts = gpu->marker_ping ?
        gpu->marker_count_b_mapped : gpu->marker_count_a_mapped;
    const dc_marker_t *markers = gpu->marker_ping ?
        gpu->marker_b_mapped : gpu->marker_a_mapped;
    chunk->marker_count = counts[slot];
    if (chunk->marker_count > DC_MARKERS_PER_CHUNK)
        return error(err, cap, "GPU marker count exceeds chunk capacity");
    memcpy(chunk->markers, markers + (size_t)slot * DC_MARKERS_PER_CHUNK,
           (size_t)chunk->marker_count * sizeof(dc_marker_t));
    chunk->particle_count = ((uint32_t *)gpu->particle_count_mapped)[slot];
    if (chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK)
        return error(err, cap, "GPU particle count exceeds chunk capacity");
    memcpy(chunk->particles,
           (dc_mpm_particle_t *)gpu->particle_mapped +
               (size_t)slot * DC_MPM_PARTICLES_PER_CHUNK,
           sizeof(chunk->particles));
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
    bool changed_mapping = old_page != (slot == UINT32_MAX ? 0u : slot + 1u);
    if (old_page && (slot == UINT32_MAX || old_page != slot + 1u))
        gpu->slot_page[old_page - 1u] = UINT32_MAX;
    pages[tile] = slot == UINT32_MAX ? 0u : slot + 1u;
    if (slot != UINT32_MAX) {
        gpu->slot_page[slot] = tile;
        if (changed_mapping && !gpu->preserve_shifted_slot[slot]) {
            dc_face_velocity_t *faces = gpu->velocity_mapped;
            const dc_face_velocity_t *source =
                gpu->chunk_velocity + (size_t)slot * DC_CHUNK_CELLS;
            for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
                memcpy(faces + ((size_t)(tile_y * DC_CHUNK_SIDE + y) * gpu->width +
                                tile_x * DC_CHUNK_SIDE),
                       source + (size_t)y * DC_CHUNK_SIDE,
                       DC_CHUNK_SIDE * sizeof(dc_face_velocity_t));
        }
        gpu->preserve_shifted_slot[slot] = false;
    }
    memcpy(gpu->slot_page_mapped, gpu->slot_page, sizeof(gpu->slot_page));
    return true;
}

bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err, uint32_t cap) {
    if (!gpu || !cells || cell_count < (uint64_t)gpu->view_width * gpu->view_height)
        return error(err, cap, "Readback buffer is too small");
    const uint32_t *source = gpu->mapped;
    for (uint32_t y = 0; y < gpu->view_height; ++y)
        memcpy(cells + (size_t)y * gpu->view_width,
               source + (size_t)(y + gpu->view_y) * gpu->width + gpu->view_x,
               (size_t)gpu->view_width * sizeof(uint32_t));
    return true;
}
