#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

uint32_t dc_gpu_host_memory_type(const VkPhysicalDeviceMemoryProperties *props,
                                 uint32_t compatible_types) {
    const VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t fallback = UINT32_MAX;
    for (uint32_t i = 0; i < props->memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = props->memoryTypes[i].propertyFlags;
        if (!(compatible_types & (1u << i)) || (flags & needed) != needed)
            continue;
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) return i;
        if (fallback == UINT32_MAX) fallback = i;
    }
    return fallback;
}

bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(gpu->device, &info, NULL, buffer) != VK_SUCCESS)
        return error(err, cap, "Cannot create GPU chunk buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu->device, *buffer, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &props);
    uint32_t type = dc_gpu_host_memory_type(&props, req.memoryTypeBits);
    if (type == UINT32_MAX) return error(err, cap, "No host-coherent GPU chunk memory");
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type };
    if (vkAllocateMemory(gpu->device, &alloc, NULL, memory) != VK_SUCCESS) {
        VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        *memory = VK_NULL_HANDLE;
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            VkMemoryPropertyFlags flags = props.memoryTypes[i].propertyFlags;
            if (!(req.memoryTypeBits & (1u << i)) || (flags & needed) != needed ||
                i == type) continue;
            alloc.memoryTypeIndex = i;
            if (vkAllocateMemory(gpu->device, &alloc, NULL, memory) == VK_SUCCESS)
                break;
        }
        if (!*memory) return error(err, cap, "Cannot allocate GPU chunk memory");
    }
    if (vkBindBufferMemory(gpu->device, *buffer, *memory, 0) != VK_SUCCESS ||
        vkMapMemory(gpu->device, *memory, 0, bytes, 0, mapped) != VK_SUCCESS)
        return error(err, cap, "Cannot allocate or map GPU chunk memory");
    if (props.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        gpu->mapped_local_bytes += req.size;
    else gpu->mapped_system_bytes += req.size;
    memset(*mapped, 0, (size_t)bytes);
    return true;
}

bool dc_gpu_make_device_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(gpu->device, &info, NULL, buffer) != VK_SUCCESS)
        return error(err, cap, "Cannot create device-local GPU buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu->device, *buffer, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if (!(req.memoryTypeBits & (1u << i)) ||
            !(props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            continue;
        VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = req.size, .memoryTypeIndex = i };
        if (vkAllocateMemory(gpu->device, &alloc, NULL, memory) == VK_SUCCESS) {
            if (vkBindBufferMemory(gpu->device, *buffer, *memory, 0) != VK_SUCCESS)
                return error(err, cap, "Cannot bind device-local GPU memory");
            gpu->device_only_bytes += req.size;
            return true;
        }
    }
    return error(err, cap, "Cannot allocate device-local GPU memory");
}

bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    gpu->chunk_velocity = calloc((size_t)gpu->slot_capacity * DC_CHUNK_CELLS,
                                  sizeof(*gpu->chunk_velocity));
    if (!gpu->chunk_velocity)
        return error(err, cap, "Cannot allocate streamed chunk velocity staging");
    gpu->page_width = (gpu->width + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    gpu->page_height = (gpu->height + DC_CHUNK_SIDE - 1u) / DC_CHUNK_SIDE;
    VkDeviceSize chunk_bytes = (VkDeviceSize)gpu->slot_capacity * DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize page_bytes = (VkDeviceSize)gpu->page_width * gpu->page_height * sizeof(uint32_t);
    VkDeviceSize particle_bytes = (VkDeviceSize)gpu->slot_capacity *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    return dc_gpu_make_device_buffer(gpu, chunk_bytes, &gpu->chunk_buffer,
               &gpu->chunk_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, chunk_bytes, &gpu->chunk_staging_buffer,
               &gpu->chunk_staging_memory, &gpu->chunk_mapped, err, cap) &&
           dc_gpu_make_device_buffer(gpu, particle_bytes, &gpu->particle_buffer,
               &gpu->particle_memory, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, particle_bytes, &gpu->particle_staging_buffer,
               &gpu->particle_staging_memory, &gpu->particle_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, gpu->slot_capacity * sizeof(uint32_t),
               &gpu->particle_count_buffer, &gpu->particle_count_memory,
               &gpu->particle_count_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, page_bytes, &gpu->page_buffer,
               &gpu->page_memory, &gpu->page_mapped, err, cap) &&
           dc_gpu_frontier_init(gpu, err, cap) &&
           dc_gpu_halo_buffers_init(gpu, err, cap) &&
           dc_gpu_fluid_buffers_init(gpu, err, cap) &&
           dc_gpu_marker_buffers_init(gpu, err, cap) &&
           dc_gpu_mpm_buffers_init(gpu, err, cap);
}

void dc_gpu_chunks_destroy(dc_gpu_t *gpu) {
    free(gpu->chunk_velocity);
    dc_gpu_frontier_destroy(gpu);
    dc_gpu_halo_destroy(gpu);
    dc_gpu_fluid_destroy(gpu);
    dc_gpu_marker_destroy(gpu);
    dc_gpu_mpm_buffers_destroy(gpu);
    if (gpu->chunk_mapped) vkUnmapMemory(gpu->device, gpu->chunk_staging_memory);
    if (gpu->particle_mapped) vkUnmapMemory(gpu->device, gpu->particle_staging_memory);
    if (gpu->particle_count_mapped) vkUnmapMemory(gpu->device, gpu->particle_count_memory);
    if (gpu->page_mapped) vkUnmapMemory(gpu->device, gpu->page_memory);
    if (gpu->chunk_buffer) vkDestroyBuffer(gpu->device, gpu->chunk_buffer, NULL);
    if (gpu->chunk_staging_buffer) vkDestroyBuffer(gpu->device, gpu->chunk_staging_buffer, NULL);
    if (gpu->particle_buffer) vkDestroyBuffer(gpu->device, gpu->particle_buffer, NULL);
    if (gpu->particle_staging_buffer) vkDestroyBuffer(gpu->device, gpu->particle_staging_buffer, NULL);
    if (gpu->particle_count_buffer) vkDestroyBuffer(gpu->device, gpu->particle_count_buffer, NULL);
    if (gpu->page_buffer) vkDestroyBuffer(gpu->device, gpu->page_buffer, NULL);
    if (gpu->chunk_memory) vkFreeMemory(gpu->device, gpu->chunk_memory, NULL);
    if (gpu->chunk_staging_memory) vkFreeMemory(gpu->device, gpu->chunk_staging_memory, NULL);
    if (gpu->particle_memory) vkFreeMemory(gpu->device, gpu->particle_memory, NULL);
    if (gpu->particle_staging_memory) vkFreeMemory(gpu->device, gpu->particle_staging_memory, NULL);
    if (gpu->particle_count_memory) vkFreeMemory(gpu->device, gpu->particle_count_memory, NULL);
    if (gpu->page_memory) vkFreeMemory(gpu->device, gpu->page_memory, NULL);
}

bool dc_gpu_copy_chunk_state(dc_gpu_t *gpu, uint32_t slot, bool upload,
                             bool particles, char *err, uint32_t cap) {
    if (!gpu || (slot >= gpu->slot_capacity && slot != UINT32_MAX) ||
        (particles && slot == UINT32_MAX))
        return error(err, cap, "Invalid chunk transfer slot");
    if (vkResetCommandBuffer(gpu->command, 0) != VK_SUCCESS)
        return error(err, cap, "Cannot reset chunk transfer command");
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(gpu->command, &begin) != VK_SUCCESS)
        return error(err, cap, "Cannot begin chunk transfer command");
    VkMemoryBarrier2 before = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = upload ? VK_PIPELINE_STAGE_2_HOST_BIT :
                                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = upload ? VK_ACCESS_2_HOST_WRITE_BIT :
                                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT |
                         VK_ACCESS_2_TRANSFER_WRITE_BIT };
    VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = &before };
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    VkDeviceSize chunk_stride = DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkBufferCopy region = { .srcOffset = slot == UINT32_MAX ? 0 : slot * chunk_stride,
        .dstOffset = slot == UINT32_MAX ? 0 : slot * chunk_stride,
        .size = slot == UINT32_MAX ? chunk_stride * gpu->slot_capacity : chunk_stride };
    vkCmdCopyBuffer(gpu->command, upload ? gpu->chunk_staging_buffer : gpu->chunk_buffer,
        upload ? gpu->chunk_buffer : gpu->chunk_staging_buffer, 1, &region);
    if (particles) {
        VkDeviceSize particle_stride = DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
        region.srcOffset = slot * particle_stride;
        region.dstOffset = region.srcOffset;
        region.size = particle_stride;
        vkCmdCopyBuffer(gpu->command,
            upload ? gpu->particle_staging_buffer : gpu->particle_buffer,
            upload ? gpu->particle_buffer : gpu->particle_staging_buffer, 1, &region);
    }
    VkMemoryBarrier2 after = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = upload ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT :
                                 VK_PIPELINE_STAGE_2_HOST_BIT,
        .dstAccessMask = upload ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT :
                                  VK_ACCESS_2_HOST_READ_BIT };
    dependency.pMemoryBarriers = &after;
    vkCmdPipelineBarrier2(gpu->command, &dependency);
    if (vkEndCommandBuffer(gpu->command) != VK_SUCCESS)
        return error(err, cap, "Cannot end chunk transfer command");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &gpu->command };
    if (vkQueueSubmit(gpu->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Chunk transfer submission failed");
    return true;
}

bool dc_gpu_upload_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                         char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= gpu->slot_capacity ||
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
             material == DC_MATERIAL_GRAVEL) && chunk->particles[i].mass_fp == 0u &&
            chunk->particles[i + DC_CHUNK_CELLS].mass_fp == 0u)
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
        if (particles[i].mass_fp == 0u && particles[i + DC_CHUNK_CELLS].mass_fp != 0u) {
            particles[i] = particles[i + DC_CHUNK_CELLS];
            memset(&particles[i + DC_CHUNK_CELLS], 0, sizeof(dc_mpm_particle_t));
        }
        if ((material == DC_MATERIAL_SAND || material == DC_MATERIAL_DIRT ||
             material == DC_MATERIAL_GRAVEL) && particles[i].mass_fp == 0u) {
            dc_chunk_particle_init(&particles[i], chunk->coord, i, material);
            ++particle_count;
        }
    }
    ((uint32_t *)gpu->particle_count_mapped)[slot] = particle_count;
    ((uint32_t *)gpu->slot_seed_mapped)[slot] = dc_chunk_particle_seed(chunk->coord);
    gpu->fluid_snapshot_valid = false;
    return dc_gpu_copy_chunk_state(gpu, slot, true, true, err, cap);
}

bool dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                           char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= gpu->slot_capacity)
        return error(err, cap, "Invalid GPU chunk download slot");
    if (!dc_gpu_copy_chunk_state(gpu, slot, false, true, err, cap)) return false;
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
        (slot != UINT32_MAX && slot >= gpu->slot_capacity))
        return error(err, cap, "Invalid GPU page mapping");
    uint32_t *pages = gpu->page_mapped;
    uint32_t tile = tile_y * gpu->page_width + tile_x;
    uint32_t new_page = slot == UINT32_MAX ? 0u : slot + 1u;
    if (pages[tile] == new_page &&
        (slot == UINT32_MAX || gpu->slot_page[slot] == tile)) {
        if (slot != UINT32_MAX) gpu->preserve_shifted_slot[slot] = false;
        return true;
    }
    if (slot != UINT32_MAX && gpu->slot_page[slot] != UINT32_MAX &&
        gpu->slot_page[slot] != tile)
        pages[gpu->slot_page[slot]] = 0u;
    uint32_t old_page = pages[tile];
    bool changed_mapping = old_page != new_page;
    if (changed_mapping) gpu->fluid_snapshot_valid = false;
    if (old_page && (slot == UINT32_MAX || old_page != slot + 1u))
        gpu->slot_page[old_page - 1u] = UINT32_MAX;
    pages[tile] = new_page;
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
