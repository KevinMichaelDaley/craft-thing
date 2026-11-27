#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/liquid_seam.h"
#include "dungeoncraft/terrain_flags.h"
#include "gpu_internal.h"
#include "renderer_internal.h"

enum {
    SEAM_TILES = 2,
    SEAM_CELLS = SEAM_TILES * DC_CHUNK_CELLS,
    OCCUPANCY_PLANES = 5,
    CELL_COVERAGE = 2,
    X_FACE_COVERAGE = 3,
    Y_FACE_COVERAGE = 4
};

enum {
    CELLS, PAGES, OCCUPANCY, PREVIOUS_VERTICAL, HEAD, VELOCITY, PRESSURE, SLOT_PAGES,
    PRESSURE_WORK, PRESSURE_STENCIL, BUFFER_COUNT
};

static const struct {
    uint32_t set, binding;
    VkDeviceSize size;
} buffer_layout[BUFFER_COUNT] = {
    [CELLS] = {0, 1, SEAM_CELLS * sizeof(dc_cell_t)},
    [PAGES] = {0, 2, SEAM_TILES * sizeof(uint32_t)},
    [OCCUPANCY] = {0, 3, OCCUPANCY_PLANES * SEAM_CELLS * sizeof(uint32_t)},
    [PREVIOUS_VERTICAL] = {0, 8, SEAM_CELLS * sizeof(uint32_t)},
    [HEAD] = {0, 9, SEAM_CELLS * sizeof(uint32_t)},
    [VELOCITY] = {0, 10, SEAM_CELLS * sizeof(dc_face_velocity_t)},
    [PRESSURE] = {0, 11, SEAM_CELLS * sizeof(float)},
    [SLOT_PAGES] = {0, 17, SEAM_TILES * sizeof(uint32_t)},
    /* Dense dispatch uses neither sparse input, but the native layout binds both. */
    [PRESSURE_WORK] = {1, 0, sizeof(uint32_t)},
    [PRESSURE_STENCIL] = {1, 1, sizeof(uint32_t)}
};

struct dc_gpu_seam_projection {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorPool pool;
    VkDescriptorSet sets[2];
    VkDeviceSize offset[BUFFER_COUNT];
    void *mapped;
};

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

void dc_gpu_seam_projection_destroy(dc_gpu_t *gpu) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    if (!scratch) return;
    if (scratch->pool) vkDestroyDescriptorPool(gpu->device, scratch->pool, NULL);
    if (scratch->mapped) vkUnmapMemory(gpu->device, scratch->memory);
    if (scratch->buffer) vkDestroyBuffer(gpu->device, scratch->buffer, NULL);
    if (scratch->memory) vkFreeMemory(gpu->device, scratch->memory, NULL);
    free(scratch);
    gpu->seam_projection = NULL;
}

static void *projection_field(const struct dc_gpu_seam_projection *scratch, unsigned field) {
    return (unsigned char *)scratch->mapped + scratch->offset[field];
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
    for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
        buffers[i] = (VkDescriptorBufferInfo){scratch->buffer, scratch->offset[i], buffer_layout[i].size};
        writes[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = scratch->sets[buffer_layout[i].set], .dstBinding = buffer_layout[i].binding,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i]};
    }
    vkUpdateDescriptorSets(gpu->device, BUFFER_COUNT, writes, 0, NULL);
    return true;
}

static bool prepare_projection(dc_gpu_t *gpu, char *err, uint32_t cap) {
    if (gpu->seam_projection) return true;
    struct dc_gpu_seam_projection *scratch = calloc(1, sizeof(*scratch));
    if (!scratch) return error(err, cap, "Cannot allocate liquid seam projection");
    gpu->seam_projection = scratch;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical, &properties);
    const VkDeviceSize align = properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize bytes = 0;
    for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
        scratch->offset[i] = (bytes + align - 1u) / align * align;
        bytes = scratch->offset[i] + buffer_layout[i].size;
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
    uint32_t *pages = projection_field(scratch, PAGES);
    uint32_t *slots = projection_field(scratch, SLOT_PAGES);
    for (uint32_t tile = 0; tile < SEAM_TILES; ++tile) {
        pages[tile] = tile + 1u;
        slots[tile] = tile;
    }
    return true;
}

static void copy_projection_tile(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
                                 dc_liquid_seam_endpoint_t endpoint, uint32_t tile,
                                 dc_liquid_seam_axis_t axis, uint32_t width) {
    const dc_gpu_t *source = endpoint.gpu;
    const VkDeviceSize cell_bytes = DC_CHUNK_CELLS * sizeof(dc_cell_t);
    const VkBufferCopy cells = {endpoint.slot * cell_bytes, scratch->offset[CELLS] + tile * cell_bytes,
                                cell_bytes};
    vkCmdCopyBuffer(command, source->chunk_buffer, scratch->buffer, 1, &cells);

    const uint32_t x = axis == DC_LIQUID_SEAM_X ? tile * DC_CHUNK_SIDE : 0u;
    const uint32_t y = axis == DC_LIQUID_SEAM_Y ? tile * DC_CHUNK_SIDE : 0u;
    VkBufferCopy velocity[DC_CHUNK_SIDE], occupancy[DC_CHUNK_SIDE];
    for (uint32_t row = 0; row < DC_CHUNK_SIDE; ++row) {
        const VkDeviceSize from = (endpoint.tile_y * DC_CHUNK_SIDE + row) *
                                  (VkDeviceSize)source->width + endpoint.tile_x * DC_CHUNK_SIDE;
        const VkDeviceSize to = (y + row) * width + x;
        velocity[row] = (VkBufferCopy){from * sizeof(dc_face_velocity_t),
            scratch->offset[VELOCITY] + to * sizeof(dc_face_velocity_t),
            DC_CHUNK_SIDE * sizeof(dc_face_velocity_t)};
        occupancy[row] = (VkBufferCopy){from * sizeof(uint32_t),
            scratch->offset[OCCUPANCY] + to * sizeof(uint32_t), DC_CHUNK_SIDE * sizeof(uint32_t)};
    }
    vkCmdCopyBuffer(command, source->velocity_buffer, scratch->buffer, DC_CHUNK_SIDE, velocity);
    const VkDeviceSize source_plane = (VkDeviceSize)source->width * source->height * sizeof(uint32_t);
    const VkDeviceSize target_plane = SEAM_CELLS * sizeof(uint32_t);
    for (uint32_t plane = 0; plane < OCCUPANCY_PLANES; ++plane) {
        vkCmdCopyBuffer(command, source->occupancy_buffer, scratch->buffer, DC_CHUNK_SIDE, occupancy);
        for (uint32_t row = 0; row < DC_CHUNK_SIDE; ++row) {
            occupancy[row].srcOffset += source_plane;
            occupancy[row].dstOffset += target_plane;
        }
    }
}

static uint32_t first_face_cell(dc_liquid_seam_axis_t axis, uint32_t lane, uint32_t width) {
    return axis == DC_LIQUID_SEAM_X ? lane * width + DC_CHUNK_SIDE - 1u :
                                      (DC_CHUNK_SIDE - 1u) * width + lane;
}

static void override_normal(dc_gpu_t *gpu, dc_liquid_seam_axis_t axis, uint32_t width,
                            const float normal[DC_CHUNK_SIDE]) {
    VkCommandBuffer command = gpu->command;
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    const size_t component = axis == DC_LIQUID_SEAM_X ? offsetof(dc_face_velocity_t, x) :
                                                       offsetof(dc_face_velocity_t, y);
    for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane) {
        const VkDeviceSize offset = scratch->offset[VELOCITY] +
            first_face_cell(axis, lane, width) * sizeof(dc_face_velocity_t) + component;
        vkCmdUpdateBuffer(command, scratch->buffer, offset, sizeof(float), &normal[lane]);
    }
}

static void read_face(const struct dc_gpu_seam_projection *scratch, dc_liquid_seam_axis_t axis,
                      uint32_t width, dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE]) {
    const dc_face_velocity_t *velocity = projection_field(scratch, VELOCITY);
    const float *pressure = projection_field(scratch, PRESSURE);
    const uint32_t *occupancy = projection_field(scratch, OCCUPANCY);
    const uint32_t *coverage = occupancy + CELL_COVERAGE * SEAM_CELLS;
    const dc_cell_t *cells = projection_field(scratch, CELLS);
    const bool y_axis = axis == DC_LIQUID_SEAM_Y;
    const uint32_t *face_coverage = occupancy + (y_axis ? Y_FACE_COVERAGE : X_FACE_COVERAGE) * SEAM_CELLS;
    for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane) {
        const uint32_t a = first_face_cell(axis, lane, width), b = a + (y_axis ? width : 1u);
        const uint32_t first_atlas = y_axis ? a : lane * DC_CHUNK_SIDE + DC_CHUNK_SIDE - 1u;
        const uint32_t second_atlas = DC_CHUNK_CELLS + (y_axis ? lane : lane * DC_CHUNK_SIDE);
        const bool open = DC_CELL_WATER_PROJECTABLE(cells[first_atlas].material) &&
            DC_CELL_WATER_PROJECTABLE(cells[second_atlas].material) &&
            coverage[a] < DC_FLUID_FULL && coverage[b] < DC_FLUID_FULL && face_coverage[a] < DC_FLUID_FULL;
        samples[lane] = (dc_liquid_seam_sample_t){y_axis ? velocity[a].y : velocity[a].x,
            pressure[a], pressure[b], open ? DC_FLUID_FULL - face_coverage[a] : 0u};
    }
}

bool dc_gpu_project_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE],
    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE], char *err, uint32_t cap) {
    if (!samples || !(seconds > 0.f) || seconds > .25f ||
        !dc_gpu_liquid_seam_mapped(first, second, axis))
        return error(err, cap, "Invalid liquid seam projection endpoints or interval");
    if (previous_normal)
        for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane)
            if (!isfinite(previous_normal[lane]))
                return error(err, cap, "Invalid previous liquid seam velocity");

    dc_gpu_t *gpu = first.gpu;
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot synchronize liquid seam projection endpoints");
    if (!prepare_projection(gpu, err, cap)) return false;
    if (!fr_renderer_begin(gpu))
        return error(err, cap, "Cannot begin liquid seam projection");

    const struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    const uint32_t width = DC_CHUNK_SIDE * (axis == DC_LIQUID_SEAM_X ? SEAM_TILES : 1u);
    const uint32_t height = SEAM_CELLS / width;
    const VkCommandBuffer command = gpu->command;
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    copy_projection_tile(command, scratch, first, 0, axis, width);
    copy_projection_tile(command, scratch, second, 1, axis, width);
    if (previous_normal) override_normal(gpu, axis, width, previous_normal);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_dense_projection(gpu, scratch->sets, width, height, seconds * 60.f);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    if (!fr_renderer_submit(gpu)) return error(err, cap, "Liquid seam projection did not complete");

    read_face(scratch, axis, width, samples);
    return true;
}
