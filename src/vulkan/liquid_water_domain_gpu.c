#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/fluid_aperture_layout.h"
#include "dungeoncraft/liquid_seam_layout.h"
#include "dungeoncraft/thermal_water_layout.h"
#include "ferrum/fire/bulk_phase.h"
#include "liquid_water_domain_gpu.h"
#include "thermal_gpu_internal.h"

_Static_assert(DC_CHUNK_SIDE == DC_WATER_SEAM_SIDE, "Water seam differs from the chunk side");
_Static_assert(DC_MARKERS_PER_CHUNK == DC_WATER_SEAM_MARKER_CAPACITY, "Water seam marker layout mismatch");
_Static_assert(sizeof(seam_crossing_t) == 2u * sizeof(uint32_t), "Water seam crossing layout mismatch");

static const struct {
    uint32_t set, binding;
    bool bound;
} buffer_layout[BUFFER_COUNT] = {
    [CELLS] = {0, 1, true},
    [PAGES] = {0, 2, true},
    [OCCUPANCY] = {0, 3, true},
    [PREVIOUS_VERTICAL] = {0, 8, true},
    [HEAD] = {0, 9, true},
    [VELOCITY] = {0, 10, true},
    [PRESSURE] = {0, 11, true},
    [MARKER_A] = {0, 12, true},
    [MARKER_B] = {0, 13, true},
    [COUNT_A] = {0, 14, true},
    [COUNT_B] = {0, 15, true},
    [MARKER_GRID] = {0, 16, true},
    [SLOT_PAGES] = {0, 17, true},
    [SLOT_SEED] = {0, 18, true},
    /* Dense dispatch still uses the native pressure descriptor layout. */
    [PRESSURE_WORK] = {1, 0, true},
    [PRESSURE_STENCIL] = {1, 1, true},
    [APERTURE_PREFIX] = {0, 28, true},
    [APERTURE_TREE] = {0, 29, true},
    [CROSSINGS] = {0, DC_WATER_SEAM_CROSSING_BINDING, true},
    [PREVIOUS_STOCK] = {0, 31, true},
    [WATER_COLOR] = {0, 32, true},
    [THERMAL_CONTROL] = {0, 36, true},
    [BULK_PHASE] = {0, 37, true},
};

static void set_buffer_sizes(struct dc_gpu_seam_projection *scratch, uint32_t width,
                             uint32_t height, uint32_t tiles) {
    const VkDeviceSize grid = (VkDeviceSize)width * height;
    const VkDeviceSize pages = grid / DC_CHUNK_CELLS;
    const VkDeviceSize atlas = (VkDeviceSize)tiles * DC_CHUNK_CELLS;
    const VkDeviceSize horizontal = (VkDeviceSize)height * dc_gpu_fluid_aperture_leaves(width);
    const VkDeviceSize vertical = (VkDeviceSize)width * dc_gpu_fluid_aperture_leaves(height);
    const VkDeviceSize tree_leaves = horizontal > vertical ? horizontal : vertical;
    const VkDeviceSize sizes[BUFFER_COUNT] = {
        [CELLS] = atlas * sizeof(dc_cell_t),
        [PAGES] = pages * sizeof(uint32_t),
        [OCCUPANCY] = OCCUPANCY_PLANES * grid * sizeof(uint32_t),
        [PREVIOUS_VERTICAL] = grid * sizeof(uint32_t),
        [HEAD] = grid * sizeof(uint32_t),
        [VELOCITY] = grid * sizeof(dc_face_velocity_t),
        [PRESSURE] = grid * sizeof(float),
        [MARKER_A] = tiles * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t),
        [MARKER_B] = tiles * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t),
        [COUNT_A] = tiles * sizeof(uint32_t),
        [COUNT_B] = tiles * sizeof(uint32_t),
        [MARKER_GRID] = atlas * sizeof(uint32_t),
        [SLOT_PAGES] = tiles * sizeof(uint32_t),
        [SLOT_SEED] = tiles * sizeof(uint32_t),
        [PRESSURE_WORK] = sizeof(uint32_t),
        [PRESSURE_STENCIL] = sizeof(uint32_t),
        [APERTURE_PREFIX] = 2u * grid * sizeof(uint32_t),
        [APERTURE_TREE] = (DC_FLUID_APERTURE_TREE_BASE + 2u * tree_leaves) * sizeof(uint32_t),
        [CROSSINGS] = atlas * sizeof(seam_crossing_t),
        [PREVIOUS_STOCK] = grid * sizeof(uint32_t),
        [WATER_COLOR] = atlas * sizeof(seam_color_t),
        [THERMAL_CONTROL] = (DC_THERMAL_WATER_COUNT_WORD + 1u) * sizeof(uint32_t),
        [INITIAL_COLOR] = atlas * sizeof(seam_color_t),
        [BULK_PHASE] = tiles * sizeof(fr_bulk_phase_chunk_t),
        [INITIAL_CELLS] = atlas * sizeof(dc_cell_t),
        [PROJECTED_VELOCITY] = grid * sizeof(dc_face_velocity_t),
        [DISPLACEMENT_CROSSINGS] = atlas * sizeof(seam_crossing_t),
        [ADVECTION_CROSSINGS] = atlas * sizeof(seam_crossing_t),
        [PUBLICATION_RECEIPT] = sizeof(uint32_t),
        [PUBLICATION_TILES] = tiles * sizeof(dc_gpu_water_tile_t),
        [PHASE_BACKUP] = 3u * grid * sizeof(uint32_t),
    };
    memcpy(scratch->size, sizes, sizeof(sizes));
}

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

void dc_gpu_seam_projection_destroy(dc_gpu_t *gpu) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    if (!scratch) return;
    for (uint32_t mode = 0; mode < 4u; ++mode)
        if (scratch->transport[mode]) vkDestroyPipeline(gpu->device, scratch->transport[mode], NULL);
    for (uint32_t mode = 0; mode < 2u; ++mode)
        if (scratch->displacement[mode]) vkDestroyPipeline(gpu->device, scratch->displacement[mode], NULL);
    if (scratch->correction) vkDestroyPipeline(gpu->device, scratch->correction, NULL);
    if (scratch->pool) vkDestroyDescriptorPool(gpu->device, scratch->pool, NULL);
    fr_renderer_compute_destroy(gpu, &scratch->publication);
    dc_gpu_address_buffer_destroy(gpu, &scratch->water_heat);
    if (scratch->mapped) vkUnmapMemory(gpu->device, scratch->memory);
    if (scratch->buffer) vkDestroyBuffer(gpu->device, scratch->buffer, NULL);
    if (scratch->memory) vkFreeMemory(gpu->device, scratch->memory, NULL);
    free(scratch);
    gpu->seam_projection = NULL;
}

static bool create_descriptors(dc_gpu_t *gpu, struct dc_gpu_seam_projection *scratch) {
    const VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, DC_GPU_DESCRIPTOR_BINDINGS + 2u};
    const VkDescriptorPoolCreateInfo pool = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &size};
    if (vkCreateDescriptorPool(gpu->device, &pool, NULL, &scratch->pool) != VK_SUCCESS) return false;
    const VkDescriptorSetLayout layouts[] = {gpu->set_layout, gpu->pressure_work_set_layout};
    const VkDescriptorSetAllocateInfo allocate = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = scratch->pool, .descriptorSetCount = 2, .pSetLayouts = layouts};
    if (vkAllocateDescriptorSets(gpu->device, &allocate, scratch->sets) != VK_SUCCESS) return false;

    VkDescriptorBufferInfo buffers[BUFFER_COUNT];
    VkWriteDescriptorSet writes[BUFFER_COUNT];
    uint32_t count = 0;
    for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
        if (!buffer_layout[i].bound) continue;
        buffers[i] = (VkDescriptorBufferInfo){scratch->buffer, scratch->offset[i], scratch->size[i]};
        writes[count++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = scratch->sets[buffer_layout[i].set], .dstBinding = buffer_layout[i].binding,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i]};
    }
    vkUpdateDescriptorSets(gpu->device, count, writes, 0, NULL);
    return true;
}

bool dc_gpu_prepare_water_projection(dc_gpu_t *gpu, uint32_t width, uint32_t height, uint32_t tiles,
    char *err, uint32_t cap) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    if (scratch && scratch->width == width && scratch->height == height && scratch->tiles == tiles)
        return true;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical, &properties);
    const VkPhysicalDeviceLimits *limits = &properties.limits;
    const VkDeviceSize grid = (VkDeviceSize)width * height;
    if (width > limits->maxComputeWorkGroupCount[0] || height > limits->maxComputeWorkGroupCount[0] ||
        (grid + 255u) / 256u > limits->maxComputeWorkGroupCount[0] ||
        tiles > limits->maxComputeWorkGroupCount[2])
        return error(err, cap, "Water domain exceeds Vulkan dispatch bounds");
    dc_gpu_seam_projection_destroy(gpu);
    scratch = calloc(1, sizeof(*scratch));
    if (!scratch) return error(err, cap, "Cannot allocate water domain");
    gpu->seam_projection = scratch;
    scratch->width = width;
    scratch->height = height;
    scratch->tiles = tiles;
    set_buffer_sizes(scratch, width, height, tiles);
    const VkDeviceSize align = properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize bytes = 0;
    for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
        scratch->offset[i] = (bytes + align - 1u) / align * align;
        if (scratch->size[i] > limits->maxStorageBufferRange) {
            dc_gpu_seam_projection_destroy(gpu);
            return error(err, cap, "Water domain exceeds Vulkan storage bounds");
        }
        bytes = scratch->offset[i] + scratch->size[i];
    }
    if (!dc_gpu_make_mapped_buffer(gpu, bytes, &scratch->buffer, &scratch->memory,
                                  &scratch->mapped, err, cap)) {
        dc_gpu_seam_projection_destroy(gpu);
        return false;
    }
    if (!create_descriptors(gpu, scratch)) {
        dc_gpu_seam_projection_destroy(gpu);
        return error(err, cap, "Cannot create liquid seam projection descriptors");
    }
    return true;
}

bool dc_gpu_prepare_water_transport(dc_gpu_t *gpu, bool occupancy, bool thermal, char *err, uint32_t cap) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    for (uint32_t mode = 0; mode < 4u; ++mode)
        if (!scratch->transport[mode] &&
            !dc_gpu_make_water_seam_pipeline(gpu, mode, mode < 2u ? scratch->height : scratch->width,
                                             &scratch->transport[mode], err, cap)) return false;
    if (!scratch->correction &&
        !dc_gpu_make_water_seam_correction_pipeline(gpu, &scratch->correction, err, cap)) return false;
    for (uint32_t mode = 0; occupancy && mode < 2u; ++mode)
        if (!scratch->displacement[mode] &&
            !dc_gpu_make_water_seam_displacement_pipeline(gpu, mode, mode ? scratch->width : scratch->height,
                                                          &scratch->displacement[mode], err, cap))
            return false;
    uint32_t *control = projection_field(scratch, THERMAL_CONTROL);
    if (!thermal) {
        control[0] = 0u;
        return true;
    }
    const uint32_t atlas_cells = scratch->tiles * DC_CHUNK_CELLS;
    if (!scratch->water_heat.buffer && !dc_gpu_address_buffer_create(gpu,
        2u * atlas_cells * sizeof(float), true, &scratch->water_heat, err, cap)) return false;
    const VkDeviceAddress address = scratch->water_heat.address;
    control[DC_THERMAL_WATER_ADDRESS_WORD] = (uint32_t)address;
    control[DC_THERMAL_WATER_ADDRESS_WORD + 1u] = (uint32_t)(address >> 32);
    control[DC_THERMAL_WATER_COUNT_WORD] = atlas_cells;
    control[0] = DC_THERMAL_WATER_MAGIC;
    return true;
}

static void copy_atlas_tile(VkCommandBuffer command, uint32_t slot,
                            VkBuffer source, VkBuffer target, VkDeviceSize target_base,
                            uint32_t tile, VkDeviceSize tile_bytes) {
    const VkBufferCopy region = {slot * tile_bytes, target_base + tile * tile_bytes, tile_bytes};
    vkCmdCopyBuffer(command, source, target, 1u, &region);
}

static void copy_transport_fields(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
                                  dc_liquid_seam_endpoint_t endpoint, uint32_t tile) {
    const dc_gpu_t *source = endpoint.gpu;
    copy_atlas_tile(command, endpoint.slot, source->chunk_buffer, scratch->buffer, scratch->offset[INITIAL_CELLS],
                    tile, DC_CHUNK_CELLS * sizeof(dc_cell_t));
    copy_atlas_tile(command, endpoint.slot, source->bulk_phase_buffer, scratch->buffer, scratch->offset[BULK_PHASE],
                    tile, sizeof(fr_bulk_phase_chunk_t));
    const VkDeviceSize color_bytes = DC_CHUNK_CELLS * sizeof(seam_color_t);
    copy_atlas_tile(command, endpoint.slot, source->water_color_buffer, scratch->buffer,
                    scratch->offset[WATER_COLOR], tile, color_bytes);
    copy_atlas_tile(command, endpoint.slot, source->water_color_buffer, scratch->buffer,
                    scratch->offset[INITIAL_COLOR], tile, color_bytes);
    const dc_gpu_seam_markers_t markers = dc_gpu_seam_active_markers(source);
    copy_atlas_tile(command, endpoint.slot, markers.records, scratch->buffer, scratch->offset[MARKER_A],
                    tile, DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t));
    copy_atlas_tile(command, endpoint.slot, markers.counts, scratch->buffer, scratch->offset[COUNT_A],
                    tile, sizeof(uint32_t));
    copy_atlas_tile(command, endpoint.slot, source->slot_seed_buffer, scratch->buffer, scratch->offset[SLOT_SEED],
                    tile, sizeof(uint32_t));

    VkBuffer live_heat = fr_thermal_gpu_water_buffer(source);
    if (live_heat) {
        const VkDeviceSize heat_bytes = DC_CHUNK_CELLS * sizeof(float);
        copy_atlas_tile(command, endpoint.slot, live_heat, scratch->water_heat.buffer, 0u, tile, heat_bytes);
        copy_atlas_tile(command, endpoint.slot, live_heat, scratch->water_heat.buffer,
                        scratch->tiles * heat_bytes, tile, heat_bytes);
    }
}

void dc_gpu_copy_projection_tile(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
    dc_liquid_seam_endpoint_t endpoint, const dc_gpu_water_tile_t *location, uint32_t tile, bool transport) {
    const dc_gpu_t *source = endpoint.gpu;
    copy_atlas_tile(command, endpoint.slot, source->chunk_buffer, scratch->buffer, scratch->offset[CELLS],
                    tile, DC_CHUNK_CELLS * sizeof(dc_cell_t));
    if (transport) copy_transport_fields(command, scratch, endpoint, tile);

    VkBufferCopy velocity[DC_CHUNK_SIDE], occupancy[DC_CHUNK_SIDE];
    for (uint32_t row = 0; row < DC_CHUNK_SIDE; ++row) {
        const VkDeviceSize from = (endpoint.tile_y * DC_CHUNK_SIDE + row) *
                                  (VkDeviceSize)source->width + endpoint.tile_x * DC_CHUNK_SIDE;
        const VkDeviceSize to = (location->scratch_y + row) * scratch->width + location->scratch_x;
        velocity[row] = (VkBufferCopy){from * sizeof(dc_face_velocity_t),
            scratch->offset[VELOCITY] + to * sizeof(dc_face_velocity_t),
            DC_CHUNK_SIDE * sizeof(dc_face_velocity_t)};
        occupancy[row] = (VkBufferCopy){from * sizeof(uint32_t),
            scratch->offset[OCCUPANCY] + to * sizeof(uint32_t), DC_CHUNK_SIDE * sizeof(uint32_t)};
    }
    vkCmdCopyBuffer(command, source->velocity_buffer, scratch->buffer, DC_CHUNK_SIDE, velocity);
    const VkDeviceSize source_plane = (VkDeviceSize)source->width * source->height * sizeof(uint32_t);
    const VkDeviceSize target_plane = (VkDeviceSize)scratch->width * scratch->height * sizeof(uint32_t);
    for (uint32_t plane = 0; plane < OCCUPANCY_PLANES; ++plane) {
        vkCmdCopyBuffer(command, source->occupancy_buffer, scratch->buffer, DC_CHUNK_SIDE, occupancy);
        for (uint32_t row = 0; row < DC_CHUNK_SIDE; ++row) {
            occupancy[row].srcOffset += source_plane;
            occupancy[row].dstOffset += target_plane;
        }
    }
}
