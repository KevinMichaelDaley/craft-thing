#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "ferrum/fire/bulk_phase.h"
#include "gpu_internal.h"
#include "gas_gpu_internal.h"
#include "reactive_gpu_internal.h"
#include "thermal_gpu_internal.h"

static bool error(char *buf, uint32_t cap, const char *message) {
    if (buf && cap) snprintf(buf, cap, "%s", message);
    return false;
}

static uint32_t visible_page_extent(uint32_t size, uint32_t tile) {
    uint32_t remaining = size - tile * DC_CHUNK_SIDE;
    return remaining < DC_CHUNK_SIDE ? remaining : DC_CHUNK_SIDE;
}

static void transfer_page_velocity(dc_gpu_t *gpu,uint32_t tile,dc_face_velocity_t *chunk,bool to_page) {
    uint32_t tile_x=tile%gpu->page_width,tile_y=tile/gpu->page_width;
    uint32_t columns=visible_page_extent(gpu->width,tile_x);
    uint32_t rows=visible_page_extent(gpu->height,tile_y);
    dc_face_velocity_t *faces=gpu->velocity_mapped;
    for(uint32_t y=0;y<rows;y++) {
        dc_face_velocity_t *view_row=faces+(size_t)(tile_y*DC_CHUNK_SIDE+y)*gpu->width+tile_x*DC_CHUNK_SIDE;
        dc_face_velocity_t *chunk_row=chunk+(size_t)y*DC_CHUNK_SIDE;
        if(to_page) memcpy(view_row,chunk_row,columns*sizeof(*faces));
        else memcpy(chunk_row,view_row,columns*sizeof(*faces));
    }
}

void dc_gpu_cache_page_velocity(dc_gpu_t *gpu,uint32_t slot) {
    uint32_t tile=gpu->slot_page[slot];
    if(tile!=UINT32_MAX && !gpu->preserve_shifted_slot[slot] && gpu->width && gpu->height)
        transfer_page_velocity(gpu,tile,gpu->chunk_velocity+(size_t)slot*DC_CHUNK_CELLS,false);
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

enum mapped_buffer_role { MAPPED_STORAGE, MAPPED_STAGING, MAPPED_TRANSFER_INITIALIZED };

static bool mapped_memory_error(char *err, uint32_t cap, const char *operation,
                                VkResult result, const VkMemoryAllocateInfo *allocation) {
    if (err && cap)
        snprintf(err, cap, "%s returned %d (%llu bytes, memory type %u)", operation,
                 (int)result, (unsigned long long)allocation->allocationSize, allocation->memoryTypeIndex);
    return false;
}

static bool make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               enum mapped_buffer_role role, bool addressable, char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (addressable) info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (vkCreateBuffer(gpu->device, &info, NULL, buffer) != VK_SUCCESS)
        return error(err, cap, "Cannot create GPU chunk buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu->device, *buffer, &req);
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(gpu->physical, &props);
    uint32_t type = dc_gpu_host_memory_type(&props, req.memoryTypeBits);
    if(role==MAPPED_STAGING) for(uint32_t i=0;i<props.memoryTypeCount;i++) {
        VkMemoryPropertyFlags flags=props.memoryTypes[i].propertyFlags;
        if((req.memoryTypeBits&(1u<<i))&&
            (flags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)&&
            !(flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {type=i;break;}
    }
    if (type == UINT32_MAX) return error(err, cap, "No host-coherent GPU chunk memory");
    const VkMemoryAllocateFlagsInfo flags = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = addressable ? &flags : NULL,
        .allocationSize = req.size, .memoryTypeIndex = type };
    VkResult result = vkAllocateMemory(gpu->device, &alloc, NULL, memory);
    if (result != VK_SUCCESS) {
        VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            VkMemoryPropertyFlags flags = props.memoryTypes[i].propertyFlags;
            if (!(req.memoryTypeBits & (1u << i)) || (flags & needed) != needed ||
                i == type) continue;
            alloc.memoryTypeIndex = i;
            result = vkAllocateMemory(gpu->device, &alloc, NULL, memory);
            if (result == VK_SUCCESS)
                break;
        }
        if (result != VK_SUCCESS) {
            *memory = VK_NULL_HANDLE;
            return mapped_memory_error(err, cap, "vkAllocateMemory", result, &alloc);
        }
    }
    result = vkBindBufferMemory(gpu->device, *buffer, *memory, 0);
    if (result != VK_SUCCESS)
        return mapped_memory_error(err, cap, "vkBindBufferMemory", result, &alloc);
    result = vkMapMemory(gpu->device, *memory, 0, bytes, 0, mapped);
    if (result != VK_SUCCESS) {
        *mapped = NULL;
        return mapped_memory_error(err, cap, "vkMapMemory", result, &alloc);
    }
    if (props.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        gpu->mapped_local_bytes += req.size;
    else gpu->mapped_system_bytes += req.size;
    if (role != MAPPED_TRANSFER_INITIALIZED) memset(*mapped, 0, (size_t)bytes);
    return true;
}
bool dc_gpu_make_mapped_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, void **mapped,
                               char *err, uint32_t cap) {
    return make_mapped_buffer(gpu,bytes,buffer,memory,mapped,MAPPED_STORAGE,false,err,cap);
}
bool dc_gpu_make_transfer_initialized_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
    VkDeviceMemory *memory, void **mapped, char *err, uint32_t cap) {
    return make_mapped_buffer(gpu,bytes,buffer,memory,mapped,MAPPED_TRANSFER_INITIALIZED,false,err,cap);
}
bool dc_gpu_make_staging_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                                VkDeviceMemory *memory, void **mapped,
                                char *err, uint32_t cap) {
    return make_mapped_buffer(gpu,bytes,buffer,memory,mapped,MAPPED_STAGING,false,err,cap);
}

static bool make_device_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, bool addressable, char *err, uint32_t cap) {
    VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bytes, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (addressable) info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
        const VkMemoryAllocateFlagsInfo flags = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
        VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = addressable ? &flags : NULL,
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

bool dc_gpu_make_device_buffer(dc_gpu_t *gpu, VkDeviceSize bytes, VkBuffer *buffer,
                               VkDeviceMemory *memory, char *err, uint32_t cap) {
    return make_device_buffer(gpu, bytes, buffer, memory, false, err, cap);
}

bool dc_gpu_make_hydro_buffer(dc_gpu_t *gpu, uint32_t field, VkDeviceSize bytes, VkBuffer *buffer,
    VkDeviceMemory *memory, void **mapped, char *err, uint32_t cap) {
    if (field >= DC_HYDRO_BUFFERS) return error(err, cap, "Invalid liquid neighborhood field");
    bool okay = mapped ? make_mapped_buffer(gpu, bytes, buffer, memory, mapped, MAPPED_STORAGE, true, err, cap) :
                         make_device_buffer(gpu, bytes, buffer, memory, true, err, cap);
    if (!okay) return false;
    VkBufferDeviceAddressInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = *buffer};
    gpu->hydro_addresses[field] = vkGetBufferDeviceAddress(gpu->device, &info);
    return gpu->hydro_addresses[field] || error(err, cap, "Cannot address liquid neighborhood field");
}

bool dc_gpu_chunks_init(dc_gpu_t *gpu, char *err, uint32_t cap) {
    VkDeviceSize face_bytes=(VkDeviceSize)dc_gpu_slot_capacity(gpu)*DC_CHUNK_CELLS*sizeof(*gpu->chunk_velocity);
    void *faces=NULL;
    if(!make_mapped_buffer(gpu,face_bytes,&gpu->chunk_velocity_buffer,&gpu->chunk_velocity_memory,
                           &faces,MAPPED_STAGING,true,err,cap)) return false;
    gpu->chunk_velocity=faces;
    VkBufferDeviceAddressInfo address={.sType=VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer=gpu->chunk_velocity_buffer};
    gpu->chunk_velocity_address=vkGetBufferDeviceAddress(gpu->device,&address);
    if(!gpu->chunk_velocity_address) return error(err,cap,"Cannot address cached water velocity");
    VkDeviceSize chunk_bytes = (VkDeviceSize)dc_gpu_slot_capacity(gpu) * DC_CHUNK_CELLS * sizeof(dc_cell_t);
    VkDeviceSize page_bytes = (VkDeviceSize)gpu->page_width * gpu->page_height * sizeof(uint32_t);
    VkDeviceSize particle_bytes = (VkDeviceSize)dc_gpu_slot_capacity(gpu) *
        DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
    return dc_gpu_make_hydro_buffer(gpu, DC_HYDRO_CELLS, chunk_bytes, &gpu->chunk_buffer,
               &gpu->chunk_memory, NULL, err, cap) &&
           dc_gpu_make_staging_buffer(gpu, chunk_bytes, &gpu->chunk_staging_buffer,
               &gpu->chunk_staging_memory, &gpu->chunk_mapped, err, cap) &&
           dc_gpu_make_hydro_buffer(gpu, DC_HYDRO_PARTICLES, particle_bytes, &gpu->particle_buffer,
               &gpu->particle_memory, NULL, err, cap) &&
           dc_gpu_make_staging_buffer(gpu, sizeof(((dc_chunk_t *)0)->particles), &gpu->particle_staging_buffer,
               &gpu->particle_staging_memory, &gpu->particle_mapped, err, cap) &&
           dc_gpu_make_mapped_buffer(gpu, dc_gpu_slot_capacity(gpu) * sizeof(uint32_t),
               &gpu->particle_count_buffer, &gpu->particle_count_memory,
               &gpu->particle_count_mapped, err, cap) &&
           dc_gpu_make_hydro_buffer(gpu, DC_HYDRO_PAGES, page_bytes, &gpu->page_buffer,
               &gpu->page_memory, &gpu->page_mapped, err, cap) &&
           dc_gpu_halo_buffers_init(gpu, err, cap) &&
           dc_gpu_fluid_buffers_init(gpu, err, cap) &&
           dc_gpu_marker_buffers_init(gpu, err, cap) &&
           dc_gpu_mpm_buffers_init(gpu, err, cap);
}

void dc_gpu_chunks_destroy(dc_gpu_t *gpu) {
    if(gpu->chunk_velocity) vkUnmapMemory(gpu->device,gpu->chunk_velocity_memory);
    if(gpu->chunk_velocity_buffer) vkDestroyBuffer(gpu->device,gpu->chunk_velocity_buffer,NULL);
    if(gpu->chunk_velocity_memory) vkFreeMemory(gpu->device,gpu->chunk_velocity_memory,NULL);
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

void dc_gpu_fluid_uploads_invalidate(dc_gpu_t *gpu) {
    memset(gpu->fluid_uploaded_state,0,sizeof(gpu->fluid_uploaded_state));
}

void dc_gpu_fluid_uploads_recheck(dc_gpu_t *gpu) {
    bool active=gpu->tick_water_source;
    for(uint32_t s=0;s<dc_gpu_slot_capacity(gpu);s++) {
        if(gpu->fluid_uploaded_state[s]==0u) return;
        active|=gpu->fluid_uploaded_state[s]==2u;
    }
    gpu->fluid_work_possible=active;
}

static void fluid_upload_completed(dc_gpu_t *gpu,uint32_t slot) {
    uint32_t first=slot==UINT32_MAX?0u:slot;
    uint32_t last=slot==UINT32_MAX?dc_gpu_slot_capacity(gpu):slot+1u;
    const dc_cell_t *cells=gpu->chunk_mapped;
    const uint32_t *markers=gpu->marker_count_a_mapped;
    for(uint32_t s=first;s<last;s++) {
        bool active=markers[s]!=0u;
        for(uint32_t i=0;i<DC_CHUNK_CELLS&&!active;i++) {
            uint32_t at=s*DC_CHUNK_CELLS+i;
            active=cells[at].fluid_mass!=0u||gpu->chunk_velocity[at].x!=0.f||gpu->chunk_velocity[at].y!=0.f;
        }
        gpu->fluid_uploaded_state[s]=active?2u:1u;
        gpu->fluid_work_possible|=active;
    }
    dc_gpu_fluid_uploads_recheck(gpu);
}

bool dc_gpu_copy_chunk_state(dc_gpu_t *gpu, uint32_t slot, bool upload,
                             bool particles, char *err, uint32_t cap) {
    if (!gpu || (slot >= dc_gpu_slot_capacity(gpu) && slot != UINT32_MAX) ||
        (particles && slot == UINT32_MAX))
        return error(err, cap, "Invalid chunk transfer slot");
    if(upload&&!particles) dc_gpu_fluid_uploads_invalidate(gpu);
    if (upload && !gpu->fluid_work_possible) {
        const dc_cell_t *cells = gpu->chunk_mapped;
        uint32_t begin = slot == UINT32_MAX ? 0u : slot * DC_CHUNK_CELLS;
        uint32_t end = slot == UINT32_MAX ? dc_gpu_slot_capacity(gpu) * DC_CHUNK_CELLS : begin + DC_CHUNK_CELLS;
        for (uint32_t i = begin; i < end; i++)
            if (cells[i].fluid_mass) { gpu->fluid_work_possible = true; break; }
    }
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
        .size = slot == UINT32_MAX ? chunk_stride * dc_gpu_slot_capacity(gpu) : chunk_stride };
    vkCmdCopyBuffer(gpu->command, upload ? gpu->chunk_staging_buffer : gpu->chunk_buffer,
        upload ? gpu->chunk_buffer : gpu->chunk_staging_buffer, 1, &region);
    if (particles) {
        VkDeviceSize particle_stride = DC_MPM_PARTICLES_PER_CHUNK * sizeof(dc_mpm_particle_t);
        region.srcOffset = upload ? 0 : slot * particle_stride;
        region.dstOffset = upload ? slot * particle_stride : 0;
        region.size = particle_stride;
        vkCmdCopyBuffer(gpu->command,
            upload ? gpu->particle_staging_buffer : gpu->particle_buffer,
            upload ? gpu->particle_buffer : gpu->particle_staging_buffer, 1, &region);
        if(upload) vkCmdCopyBuffer(gpu->command,gpu->particle_staging_buffer,gpu->mpm_output_buffer,1,&region);
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
    if(upload&&particles) fluid_upload_completed(gpu,slot);
    return true;
}

bool dc_gpu_upload_chunk_exact(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                              bool seed, char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= dc_gpu_slot_capacity(gpu) ||
        chunk->marker_count > DC_MARKERS_PER_CHUNK ||
        chunk->particle_count > DC_MPM_PARTICLES_PER_CHUNK)
        return error(err, cap, "Invalid GPU chunk upload slot");
    if (dc_gpu_liquid_owner_held(gpu))
        return error(err, cap, "Cannot replace a chunk during a coupled water cycle");
    uint32_t particle_count = 0, missing_primary = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        particle_count += chunk->particles[i].mass_fp != 0u;
    if (particle_count != chunk->particle_count)
        return error(err, cap, "Chunk particle count does not match active records");
    fr_gas_gpu_geometry_changed(gpu,particle_count!=0u);
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t material = chunk->cells[i].material;
        if (seed && (material == DC_MATERIAL_SAND || material == DC_MATERIAL_DIRT ||
             material == DC_MATERIAL_GRAVEL || material == DC_MATERIAL_MORTAR_DEBRIS || material == DC_MATERIAL_LEAF_DEBRIS) && chunk->particles[i].mass_fp == 0u &&
            chunk->particles[i + DC_CHUNK_CELLS].mass_fp == 0u)
            ++missing_primary;
    }
    if (missing_primary > DC_MPM_PARTICLES_PER_CHUNK - particle_count)
        return error(err, cap, "GPU particle pool is full");
    if(!gpu->reactive_checkpoint_installing&&!fr_gpu_reactive_clear_chunk(gpu,slot,err,cap)) return false;
    if (!gpu->fluid_work_possible) {
        gpu->fluid_work_possible = chunk->marker_count != 0u;
        for (uint32_t i = 0; i < DC_CHUNK_CELLS && !gpu->fluid_work_possible; i++)
            gpu->fluid_work_possible = chunk->face_velocity[i].x != 0.f || chunk->face_velocity[i].y != 0.f;
    }
    dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(cells + (size_t)slot * DC_CHUNK_CELLS, chunk->cells, sizeof(chunk->cells));
    memset((fr_bulk_phase_chunk_t *)gpu->bulk_phase_mapped+slot,0,sizeof(fr_bulk_phase_chunk_t));
    memset((float*)gpu->water_color_mapped+(size_t)slot*DC_CHUNK_CELLS*4,0,DC_CHUNK_CELLS*16);
    memset((float*)gpu->shape_color_mapped+(size_t)slot*DC_CHUNK_CELLS*4,0,DC_CHUNK_CELLS*16);
    for (uint32_t i=0;i<DC_CHUNK_CELLS;i++)
        if ((chunk->cells[i].flags & (DC_CELL_LOADED_TERRAIN|DC_CELL_BONDS_READY)) == DC_CELL_LOADED_TERRAIN)
            gpu->component_baseline_pending=true;
    memcpy(gpu->chunk_velocity + (size_t)slot * DC_CHUNK_CELLS,
           chunk->face_velocity, sizeof(chunk->face_velocity));
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
    dc_mpm_particle_t *particles = gpu->particle_mapped;
    memcpy(particles, chunk->particles, sizeof(chunk->particles));
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
        uint32_t material = chunk->cells[i].material;
        if (seed && particles[i].mass_fp == 0u && particles[i + DC_CHUNK_CELLS].mass_fp != 0u) {
            particles[i] = particles[i + DC_CHUNK_CELLS];
            memset(&particles[i + DC_CHUNK_CELLS], 0, sizeof(dc_mpm_particle_t));
        }
        if (seed && (material == DC_MATERIAL_SAND || material == DC_MATERIAL_DIRT ||
             material == DC_MATERIAL_GRAVEL || material == DC_MATERIAL_MORTAR_DEBRIS || material == DC_MATERIAL_LEAF_DEBRIS) && particles[i].mass_fp == 0u) {
            dc_chunk_particle_init(&particles[i], chunk->coord, i, material);
            ++particle_count;
        }
    }
    ((uint32_t *)gpu->particle_count_mapped)[slot] = particle_count;
    ((uint32_t *)gpu->slot_seed_mapped)[slot] = dc_chunk_particle_seed(chunk->coord);
    dc_gpu_structural_track_chunk(gpu, slot, chunk->coord);
    gpu->fluid_snapshot_valid = false;
    bool okay=dc_gpu_copy_chunk_state(gpu, slot, true, true, err, cap);
    if(okay && gpu->slot_page[slot]!=UINT32_MAX && !gpu->preserve_shifted_slot[slot])
        transfer_page_velocity(gpu,gpu->slot_page[slot],gpu->chunk_velocity+(size_t)slot*DC_CHUNK_CELLS,true);
    if(okay&&!gpu->reactive_checkpoint_installing) fr_thermal_clear_water_chunk(gpu,slot);
    return okay;
}

bool dc_gpu_upload_chunk(dc_gpu_t *gpu, uint32_t slot, const dc_chunk_t *chunk,
                         char *err, uint32_t cap) {
    return dc_gpu_upload_chunk_checkpoint(gpu, slot, chunk, true, NULL, NULL, err, cap);
}

bool dc_gpu_download_chunk(dc_gpu_t *gpu, uint32_t slot, dc_chunk_t *chunk,
                           char *err, uint32_t cap) {
    if (!gpu || !chunk || slot >= dc_gpu_slot_capacity(gpu))
        return error(err, cap, "Invalid GPU chunk download slot");
    if (!dc_gpu_copy_chunk_state(gpu, slot, false, true, err, cap)) return false;
    const dc_cell_t *cells = gpu->chunk_mapped;
    memcpy(chunk->cells, cells + (size_t)slot * DC_CHUNK_CELLS, sizeof(chunk->cells));
    uint32_t tile = gpu->slot_page[slot];
    memcpy(chunk->face_velocity,
           gpu->chunk_velocity + (size_t)slot * DC_CHUNK_CELLS,
           sizeof(chunk->face_velocity));
    if (tile != UINT32_MAX && !gpu->preserve_shifted_slot[slot])
        transfer_page_velocity(gpu,tile,chunk->face_velocity,false);
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
    memcpy(chunk->particles, gpu->particle_mapped, sizeof(chunk->particles));
    return true;
}

bool dc_gpu_set_page(dc_gpu_t *gpu, uint32_t tile_x, uint32_t tile_y,
                     uint32_t slot, char *err, uint32_t cap) {
    if (!gpu || tile_x >= gpu->page_width || tile_y >= gpu->page_height ||
        (slot != UINT32_MAX && slot >= dc_gpu_slot_capacity(gpu)))
        return error(err, cap, "Invalid GPU page mapping");
    uint32_t *pages = gpu->page_mapped;
    uint32_t tile = tile_y * gpu->page_width + tile_x;
    uint32_t new_page = slot == UINT32_MAX ? 0u : slot + 1u;
    if (pages[tile] == new_page &&
        (slot == UINT32_MAX || gpu->slot_page[slot] == tile)) {
        if (slot != UINT32_MAX && gpu->preserve_shifted_slot[slot]) {
            transfer_page_velocity(gpu,tile,gpu->chunk_velocity+(size_t)slot*DC_CHUNK_CELLS,true);
            gpu->preserve_shifted_slot[slot]=false;
        }
        return true;
    }
    if (dc_gpu_liquid_owner_held(gpu))
        return error(err, cap, "Cannot change page mappings during a coupled water cycle");
    fr_gas_gpu_invalidate_cache(gpu);
    gpu->body_refresh_pending = true;
    if(slot!=UINT32_MAX) dc_gpu_cache_page_velocity(gpu,slot);
    if(pages[tile]) dc_gpu_cache_page_velocity(gpu,pages[tile]-1u);
    if (slot != UINT32_MAX && gpu->slot_page[slot] != UINT32_MAX &&
        gpu->slot_page[slot] != tile)
        pages[gpu->slot_page[slot]] = 0u;
    uint32_t old_page = pages[tile];
    bool changed_mapping = old_page != new_page;
    if (changed_mapping) dc_gpu_structural_data(gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = 1;
    if (changed_mapping) gpu->fluid_snapshot_valid = false;
    if (old_page && (slot == UINT32_MAX || old_page != slot + 1u))
        gpu->slot_page[old_page - 1u] = UINT32_MAX;
    pages[tile] = new_page;
    if (slot != UINT32_MAX) {
        gpu->slot_page[slot] = tile;
        if(changed_mapping && gpu->width && gpu->height)
            transfer_page_velocity(gpu,tile,gpu->chunk_velocity+(size_t)slot*DC_CHUNK_CELLS,true);
        gpu->preserve_shifted_slot[slot] = false;
    }
    memcpy(gpu->slot_page_mapped, gpu->slot_page,
           dc_gpu_slot_capacity(gpu) * sizeof(gpu->slot_page[0]));
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

void dc_gpu_structural_track_chunk(dc_gpu_t *gpu, uint32_t slot,
                                   dc_chunk_coord_t coord) {
    if (!gpu || !gpu->body_mapped || slot >= dc_gpu_slot_capacity(gpu))
        return;
    uint32_t *p =
        dc_gpu_structural_data(gpu) + DC_STRUCTURAL_HEADER_WORDS + slot * 8u;
    for (unsigned other = 0; other < dc_gpu_slot_capacity(gpu); other++) {
        uint32_t *previous = dc_gpu_structural_data(gpu) +
                             DC_STRUCTURAL_HEADER_WORDS + other * 8u;
        if (other != slot && previous[5] &&
            memcmp(previous, &coord, sizeof(coord)) == 0)
            previous[5] = 0;
    }
    dc_gpu_structural_data(gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD] = 1;
    memcpy(p, &coord, sizeof(coord));
    ++p[4];
    p[5] = 1;
}

uint32_t dc_gpu_find_active_chunk(dc_gpu_t *gpu, dc_chunk_coord_t coord) {
    const uint32_t *registry = dc_gpu_structural_data(gpu) + DC_STRUCTURAL_HEADER_WORDS;
    for (uint32_t slot = 0; slot < dc_gpu_slot_capacity(gpu); ++slot) {
        const uint32_t *owner = registry + slot * 8u;
        if (gpu->slot_page[slot] != UINT32_MAX && owner[5] &&
            memcmp(owner, &coord, sizeof(coord)) == 0)
            return slot;
    }
    return UINT32_MAX;
}
