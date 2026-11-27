#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/fluid_aperture_layout.h"
#include "dungeoncraft/liquid_seam.h"
#include "dungeoncraft/liquid_seam_layout.h"
#include "dungeoncraft/terrain_flags.h"
#include "dungeoncraft/thermal_water_layout.h"
#include "ferrum/fire/bulk_phase.h"
#include "body_address_gpu.h"
#include "gpu_internal.h"
#include "liquid_seam_gpu_internal.h"
#include "thermal_gpu_internal.h"

enum {
    SEAM_TILES = 2,
    SEAM_CELLS = SEAM_TILES * DC_CHUNK_CELLS,
    OCCUPANCY_PLANES = 5,
    CELL_COVERAGE = 2,
    X_FACE_COVERAGE = 3,
    Y_FACE_COVERAGE = 4,
    READBACK_ONLY = 2
};

typedef struct {
    uint32_t sent, received;
} seam_crossing_t;
typedef float seam_color_t[4];

_Static_assert(DC_CHUNK_SIDE == DC_WATER_SEAM_SIDE, "Water seam differs from the chunk side");
_Static_assert(DC_MARKERS_PER_CHUNK == DC_WATER_SEAM_MARKER_CAPACITY, "Water seam marker layout mismatch");
_Static_assert(sizeof(seam_crossing_t) == 2u * sizeof(uint32_t), "Water seam crossing layout mismatch");

enum {
    CELLS, PAGES, OCCUPANCY, PREVIOUS_VERTICAL, HEAD, VELOCITY, PRESSURE, SLOT_PAGES,
    PRESSURE_WORK, PRESSURE_STENCIL, INITIAL_CELLS, APERTURE_PREFIX, APERTURE_TREE,
    CROSSINGS, WATER_COLOR, INITIAL_COLOR, THERMAL_CONTROL, BULK_PHASE, PUBLICATION_RECEIPT,
    MARKER_A, MARKER_B, COUNT_A, COUNT_B, MARKER_GRID, SLOT_SEED, PREVIOUS_STOCK,
    PROJECTED_VELOCITY, ADVECTION_CROSSINGS, BUFFER_COUNT
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
    [MARKER_A] = {0, 12, SEAM_TILES * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t)},
    [MARKER_B] = {0, 13, SEAM_TILES * DC_MARKERS_PER_CHUNK * sizeof(dc_marker_t)},
    [COUNT_A] = {0, 14, SEAM_TILES * sizeof(uint32_t)},
    [COUNT_B] = {0, 15, SEAM_TILES * sizeof(uint32_t)},
    [MARKER_GRID] = {0, 16, SEAM_CELLS * sizeof(uint32_t)},
    [SLOT_PAGES] = {0, 17, SEAM_TILES * sizeof(uint32_t)},
    [SLOT_SEED] = {0, 18, SEAM_TILES * sizeof(uint32_t)},
    /* Dense dispatch uses neither sparse input, but the native layout binds both. */
    [PRESSURE_WORK] = {1, 0, sizeof(uint32_t)},
    [PRESSURE_STENCIL] = {1, 1, sizeof(uint32_t)},
    [APERTURE_PREFIX] = {0, 28, 2u * SEAM_CELLS * sizeof(uint32_t)},
    [APERTURE_TREE] = {0, 29, (DC_FLUID_APERTURE_TREE_BASE +
        2u * SEAM_CELLS / DC_FLUID_APERTURE_TILE_CELLS) * sizeof(uint32_t)},
    [CROSSINGS] = {0, DC_WATER_SEAM_CROSSING_BINDING, SEAM_CELLS * sizeof(seam_crossing_t)},
    [PREVIOUS_STOCK] = {0, 31, SEAM_CELLS * sizeof(uint32_t)},
    [WATER_COLOR] = {0, 32, SEAM_CELLS * sizeof(seam_color_t)},
    [THERMAL_CONTROL] = {0, 36, (DC_THERMAL_WATER_COUNT_WORD + 1u) * sizeof(uint32_t)},
    [INITIAL_COLOR] = {READBACK_ONLY, 0, SEAM_CELLS * sizeof(seam_color_t)},
    [BULK_PHASE] = {0, 37, SEAM_TILES * sizeof(fr_bulk_phase_chunk_t)},
    [INITIAL_CELLS] = {READBACK_ONLY, 0, SEAM_CELLS * sizeof(dc_cell_t)},
    [PROJECTED_VELOCITY] = {READBACK_ONLY, 0, SEAM_CELLS * sizeof(dc_face_velocity_t)},
    [ADVECTION_CROSSINGS] = {READBACK_ONLY, 0, SEAM_CELLS * sizeof(seam_crossing_t)},
    [PUBLICATION_RECEIPT] = {READBACK_ONLY, 0, sizeof(uint32_t)}
};

struct dc_gpu_seam_projection {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorPool pool;
    VkDescriptorSet sets[2];
    VkPipeline transport[4];
    VkPipeline correction;
    fr_renderer_compute_t publication;
    dc_gpu_address_buffer_t water_heat;
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
    for (uint32_t mode = 0; mode < 4u; ++mode)
        if (scratch->transport[mode]) vkDestroyPipeline(gpu->device, scratch->transport[mode], NULL);
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
    uint32_t count = 0;
    for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
        if (buffer_layout[i].set == READBACK_ONLY) continue;
        buffers[i] = (VkDescriptorBufferInfo){scratch->buffer, scratch->offset[i], buffer_layout[i].size};
        writes[count++] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = scratch->sets[buffer_layout[i].set], .dstBinding = buffer_layout[i].binding,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffers[i]};
    }
    vkUpdateDescriptorSets(gpu->device, count, writes, 0, NULL);
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

static bool prepare_transport(dc_gpu_t *gpu, bool thermal, char *err, uint32_t cap) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    for (uint32_t mode = 0; mode < 4u; ++mode)
        if (!scratch->transport[mode] &&
            !dc_gpu_make_water_seam_pipeline(gpu, mode, &scratch->transport[mode], err, cap)) return false;
    if (!scratch->correction &&
        !dc_gpu_make_water_seam_correction_pipeline(gpu, &scratch->correction, err, cap)) return false;
    uint32_t *control = projection_field(scratch, THERMAL_CONTROL);
    if (!thermal) {
        control[0] = 0u;
        return true;
    }
    if (!scratch->water_heat.buffer && !dc_gpu_address_buffer_create(gpu,
        2u * SEAM_CELLS * sizeof(float), true, &scratch->water_heat, err, cap)) return false;
    const VkDeviceAddress address = scratch->water_heat.address;
    control[DC_THERMAL_WATER_ADDRESS_WORD] = (uint32_t)address;
    control[DC_THERMAL_WATER_ADDRESS_WORD + 1u] = (uint32_t)(address >> 32);
    control[DC_THERMAL_WATER_COUNT_WORD] = SEAM_CELLS;
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
                        SEAM_TILES * heat_bytes, tile, heat_bytes);
    }
}

static void copy_projection_tile(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
                                 dc_liquid_seam_endpoint_t endpoint, uint32_t tile,
                                 dc_liquid_seam_axis_t axis, uint32_t width, bool transport) {
    const dc_gpu_t *source = endpoint.gpu;
    copy_atlas_tile(command, endpoint.slot, source->chunk_buffer, scratch->buffer, scratch->offset[CELLS],
                    tile, DC_CHUNK_CELLS * sizeof(dc_cell_t));
    if (transport) copy_transport_fields(command, scratch, endpoint, tile);

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
                      uint32_t width, bool corrected, dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE]) {
    const dc_face_velocity_t *velocity = projection_field(scratch, corrected ? PROJECTED_VELOCITY : VELOCITY);
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

static void read_water_plan(const struct dc_gpu_seam_projection *scratch, dc_liquid_seam_axis_t axis,
                            uint32_t width, bool thermal, dc_water_seam_plan_t *plan) {
    const dc_cell_t *initial = projection_field(scratch, INITIAL_CELLS);
    const dc_cell_t *final = projection_field(scratch, CELLS);
    const seam_crossing_t *crossings = projection_field(scratch, CROSSINGS);
    const seam_crossing_t *advected = projection_field(scratch, ADVECTION_CROSSINGS);
    const seam_color_t *initial_color = projection_field(scratch, INITIAL_COLOR);
    const seam_color_t *final_color = projection_field(scratch, WATER_COLOR);
    const float *heat = thermal ? scratch->water_heat.mapped : NULL;
    plan->thermal_enabled = thermal;
    read_face(scratch, axis, width, true, plan->projection);
    memset(plan->crossing_fp, 0, sizeof(plan->crossing_fp));
    memset(plan->advected_fp, 0, sizeof(plan->advected_fp));
    for (uint32_t tile = 0; tile < SEAM_TILES; ++tile)
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) {
            uint32_t atlas = tile * DC_CHUNK_CELLS + i;
            uint32_t sent = crossings[atlas].sent, received = crossings[atlas].received;
            plan->cells[tile][i] = (dc_water_seam_cell_t){initial[atlas].fluid_mass,
                final[atlas].fluid_mass, sent, received};
            dc_water_seam_payload_t *payload = &plan->payload[tile][i];
            memcpy(payload->initial_color, initial_color[atlas], sizeof(payload->initial_color));
            memcpy(payload->final_color, final_color[atlas], sizeof(payload->final_color));
            payload->initial_excess_heat = heat ? heat[SEAM_CELLS + atlas] : 0.f;
            payload->final_excess_heat = heat ? heat[atlas] : 0.f;
            if (tile == 0) {
                uint32_t lane = axis == DC_LIQUID_SEAM_X ? i / DC_CHUNK_SIDE : i % DC_CHUNK_SIDE;
                plan->crossing_fp[lane] += (int32_t)sent - (int32_t)received;
                plan->advected_fp[lane] += (int32_t)advected[atlas].sent - (int32_t)advected[atlas].received;
            }
        }
    for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane) {
        const dc_liquid_seam_sample_t *face = &plan->projection[lane];
        plan->pressure_work[lane] = (face->first_pressure - face->second_pressure) *
                                    ((float)plan->advected_fp[lane] / DC_FLUID_FULL);
    }
}

static bool prepare_publication(dc_gpu_t *gpu, dc_gpu_t *peer, bool thermal,
                                char *err, uint32_t cap) {
    struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    const unsigned fields[] = {PUBLICATION_RECEIPT, INITIAL_CELLS, CELLS, INITIAL_COLOR, WATER_COLOR,
                               VELOCITY, OCCUPANCY, BULK_PHASE, THERMAL_CONTROL, MARKER_B, COUNT_B, MARKER_GRID};
    _Static_assert(sizeof(fields) / sizeof(fields[0]) == DC_WATER_PUBLICATION_SCRATCH_BINDINGS,
                   "Water publication scratch binding count mismatch");
    VkDescriptorBufferInfo buffers[DC_WATER_PUBLICATION_SCRATCH_BINDINGS];
    for (unsigned i = 0; i < DC_WATER_PUBLICATION_SCRATCH_BINDINGS; ++i) {
        unsigned field = fields[i];
        buffers[i] = (VkDescriptorBufferInfo){scratch->buffer, scratch->offset[field], buffer_layout[field].size};
    }
    if (thermal)
        buffers[8] = (VkDescriptorBufferInfo){scratch->water_heat.buffer, 0, scratch->water_heat.bytes};
    return dc_gpu_prepare_liquid_receipt(gpu, err, cap) &&
           dc_gpu_prepare_water_publication(gpu, peer, &scratch->publication, buffers, err, cap);
}

static void record_advection_readback(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch) {
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    const VkBufferCopy regions[] = {
        {scratch->offset[VELOCITY], scratch->offset[PROJECTED_VELOCITY], buffer_layout[VELOCITY].size},
        {scratch->offset[CROSSINGS], scratch->offset[ADVECTION_CROSSINGS], buffer_layout[CROSSINGS].size}
    };
    vkCmdCopyBuffer(command, scratch->buffer, scratch->buffer, 2u, regions);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COPY_BIT, 0, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0);
}

static dc_liquid_seam_result_t run_seam(dc_liquid_seam_endpoint_t first, dc_liquid_seam_endpoint_t second,
    dc_liquid_seam_axis_t axis, float seconds, const float *previous_normal,
    dc_liquid_seam_sample_t *samples, dc_water_seam_plan_t *plan, bool publish, char *err, uint32_t cap) {
    if ((!samples && !plan && !publish) || !(seconds > 0.f) || seconds > .25f ||
        !dc_gpu_liquid_seam_mapped(first, second, axis)) {
        error(err, cap, "Invalid liquid seam projection endpoints or interval");
        return DC_LIQUID_SEAM_REJECTED;
    }
    if (dc_gpu_liquid_seam_pending(first.gpu) || dc_gpu_liquid_seam_pending(second.gpu)) {
        error(err, cap, "A liquid seam receipt is still pending");
        return DC_LIQUID_SEAM_UNCONFIRMED;
    }
    if (previous_normal)
        for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane)
            if (!isfinite(previous_normal[lane])) {
                error(err, cap, "Invalid previous liquid seam velocity");
                return DC_LIQUID_SEAM_REJECTED;
            }

    dc_gpu_t *gpu = first.gpu;
    const bool transport = plan || publish;
    if (transport && (gpu->marker_correction != second.gpu->marker_correction ||
        (gpu->marker_correction && ((gpu->fluid_tick ^ second.gpu->fluid_tick) & 1u)))) {
        error(err, cap, "Liquid seam owners have different marker correction settings or parity");
        return DC_LIQUID_SEAM_REJECTED;
    }
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS) {
        error(err, cap, "Cannot synchronize liquid seam projection endpoints");
        return DC_LIQUID_SEAM_REJECTED;
    }
    const bool thermal = gpu->thermal != NULL;
    if (!prepare_projection(gpu, err, cap) ||
        (transport && !prepare_transport(gpu, thermal, err, cap)) ||
        (publish && !prepare_publication(gpu, second.gpu, thermal, err, cap)))
        return DC_LIQUID_SEAM_REJECTED;
    if (!fr_renderer_begin(gpu)) {
        error(err, cap, "Cannot begin liquid seam projection");
        return DC_LIQUID_SEAM_REJECTED;
    }

    const struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    uint32_t *receipt = projection_field(scratch, PUBLICATION_RECEIPT);
    if (publish) *receipt = 0u;
    const uint32_t width = DC_CHUNK_SIDE * (axis == DC_LIQUID_SEAM_X ? SEAM_TILES : 1u);
    const uint32_t height = SEAM_CELLS / width;
    const VkCommandBuffer command = gpu->command;
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    copy_projection_tile(command, scratch, first, 0, axis, width, transport);
    copy_projection_tile(command, scratch, second, 1, axis, width, transport);
    if (transport)
        vkCmdFillBuffer(command, scratch->buffer, scratch->offset[CROSSINGS], buffer_layout[CROSSINGS].size, 0u);
    if (previous_normal) override_normal(gpu, axis, width, previous_normal);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dc_gpu_record_dense_projection(gpu, scratch->sets, width, height, seconds * 60.f);
    if (transport) {
        const uint32_t seam_axis = axis == DC_LIQUID_SEAM_X ? DC_WATER_SEAM_HORIZONTAL : DC_WATER_SEAM_VERTICAL;
        dc_gpu_record_water_seam_transport(gpu, scratch->sets[0], scratch->transport, width, height,
            seconds * 60.f, first.gpu->rigid_occupancy_present || second.gpu->rigid_occupancy_present,
            seam_axis);
        record_advection_readback(command, scratch);
        dc_gpu_record_water_seam_correction(gpu, scratch->sets[0], scratch->correction,
                                            width, height, seconds * 60.f, seam_axis);
    }
    if (publish)
        dc_gpu_record_water_publication(gpu, &scratch->publication, dc_gpu_seam_push(first, second, axis), thermal);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT);

    if (publish) {
        dc_liquid_seam_result_t result = dc_gpu_submit_liquid_seam(gpu, second.gpu, receipt, false, err, cap);
        if (result != DC_LIQUID_SEAM_COMMITTED) return result;
    } else if (!fr_renderer_submit(gpu)) {
        error(err, cap, "Liquid seam projection did not complete");
        return DC_LIQUID_SEAM_REJECTED;
    }

    if (plan) read_water_plan(scratch, axis, width, thermal, plan);
    else if (samples) read_face(scratch, axis, width, false, samples);
    return DC_LIQUID_SEAM_COMMITTED;
}

bool dc_gpu_project_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE],
    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE], char *err, uint32_t cap) {
    return run_seam(first, second, axis, seconds, previous_normal, samples, NULL, false, err, cap)
        == DC_LIQUID_SEAM_COMMITTED;
}

bool dc_gpu_plan_water_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE], dc_water_seam_plan_t *plan,
    char *err, uint32_t cap) {
    return run_seam(first, second, axis, seconds, previous_normal, NULL, plan, false, err, cap)
        == DC_LIQUID_SEAM_COMMITTED;
}

dc_liquid_seam_result_t dc_gpu_execute_water_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis, float seconds,
    const float previous_normal[DC_CHUNK_SIDE], dc_water_seam_plan_t *plan,
    char *err, uint32_t cap) {
    return run_seam(first, second, axis, seconds, previous_normal, NULL, plan, true, err, cap);
}
