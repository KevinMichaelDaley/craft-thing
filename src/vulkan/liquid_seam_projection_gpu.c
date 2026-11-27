#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam_layout.h"
#include "dungeoncraft/terrain_flags.h"
#include "liquid_water_domain_gpu.h"

enum {
    SEAM_CELLS = SEAM_TILES * DC_CHUNK_CELLS,
    MAX_WATER_COHORT_TILES = 2u * DC_GPU_CHUNK_SLOTS
};

typedef struct {
    dc_gpu_t *gpu, *peer;
    const dc_liquid_seam_endpoint_t *tiles;
    const dc_gpu_water_tile_t *locations;
    uint32_t count, width, height;
} water_domain_t;

static bool error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
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
    const seam_crossing_t *displaced = projection_field(scratch, DISPLACEMENT_CROSSINGS);
    const seam_crossing_t *advected = projection_field(scratch, ADVECTION_CROSSINGS);
    const seam_color_t *initial_color = projection_field(scratch, INITIAL_COLOR);
    const seam_color_t *final_color = projection_field(scratch, WATER_COLOR);
    const float *heat = thermal ? scratch->water_heat.mapped : NULL;
    plan->thermal_enabled = thermal;
    read_face(scratch, axis, width, true, plan->projection);
    memset(plan->crossing_fp, 0, sizeof(plan->crossing_fp));
    memset(plan->displaced_fp, 0, sizeof(plan->displaced_fp));
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
                const int32_t forced = (int32_t)displaced[atlas].sent - (int32_t)displaced[atlas].received;
                plan->displaced_fp[lane] += forced;
                plan->advected_fp[lane] += (int32_t)advected[atlas].sent - (int32_t)advected[atlas].received - forced;
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
                               VELOCITY, OCCUPANCY, BULK_PHASE, THERMAL_CONTROL, MARKER_B, COUNT_B, MARKER_GRID,
                               PUBLICATION_TILES};
    _Static_assert(sizeof(fields) / sizeof(fields[0]) == DC_WATER_PUBLICATION_SCRATCH_BINDINGS,
                   "Water publication scratch binding count mismatch");
    VkDescriptorBufferInfo buffers[DC_WATER_PUBLICATION_SCRATCH_BINDINGS];
    for (unsigned i = 0; i < DC_WATER_PUBLICATION_SCRATCH_BINDINGS; ++i) {
        unsigned field = fields[i];
        buffers[i] = (VkDescriptorBufferInfo){scratch->buffer, scratch->offset[field], scratch->size[field]};
    }
    if (thermal)
        buffers[8] = (VkDescriptorBufferInfo){scratch->water_heat.buffer, 0, scratch->water_heat.bytes};
    return dc_gpu_prepare_liquid_receipt(gpu, err, cap) &&
           dc_gpu_prepare_water_publication(gpu, peer, &scratch->publication, buffers, err, cap);
}

static void record_stage_readback(VkCommandBuffer command, const struct dc_gpu_seam_projection *scratch,
                                  unsigned crossing_snapshot, bool include_velocity) {
    const VkBufferCopy regions[] = {
        {scratch->offset[CROSSINGS], scratch->offset[crossing_snapshot], scratch->size[CROSSINGS]},
        {scratch->offset[VELOCITY], scratch->offset[PROJECTED_VELOCITY], scratch->size[VELOCITY]}
    };
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    vkCmdCopyBuffer(command, scratch->buffer, scratch->buffer, include_velocity ? 2u : 1u, regions);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COPY_BIT, 0, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0);
}

static dc_liquid_seam_result_t run_water_domain(const water_domain_t *domain,
    dc_liquid_seam_axis_t axis, float seconds, const float *previous_normal,
    dc_liquid_seam_sample_t *samples, dc_water_seam_plan_t *plan, bool publish, char *err, uint32_t cap) {
    if ((!samples && !plan && !publish) || !(seconds > 0.f) || seconds > .25f) {
        error(err, cap, "Invalid liquid seam projection endpoints or interval");
        return DC_LIQUID_SEAM_REJECTED;
    }
    dc_gpu_t *gpu = domain->gpu, *peer = domain->peer;
    if (dc_gpu_liquid_seam_pending(gpu) || dc_gpu_liquid_seam_pending(peer)) {
        error(err, cap, "A liquid seam receipt is still pending");
        return DC_LIQUID_SEAM_UNCONFIRMED;
    }
    if (previous_normal)
        for (uint32_t lane = 0; lane < DC_CHUNK_SIDE; ++lane)
            if (!isfinite(previous_normal[lane])) {
                error(err, cap, "Invalid previous liquid seam velocity");
                return DC_LIQUID_SEAM_REJECTED;
            }

    const bool transport = plan || publish;
    if (transport && (gpu->marker_correction != peer->marker_correction ||
        (gpu->marker_correction && ((gpu->fluid_tick ^ peer->fluid_tick) & 1u)))) {
        error(err, cap, "Liquid seam owners have different marker correction settings or parity");
        return DC_LIQUID_SEAM_REJECTED;
    }
    if (vkQueueWaitIdle(gpu->queue) != VK_SUCCESS) {
        error(err, cap, "Cannot synchronize liquid seam projection endpoints");
        return DC_LIQUID_SEAM_REJECTED;
    }
    const bool thermal = gpu->thermal != NULL;
    const bool occupancy = gpu->rigid_occupancy_present || peer->rigid_occupancy_present;
    const uint32_t width = domain->width, height = domain->height, count = domain->count;
    if (!dc_gpu_prepare_water_projection(gpu, width, height, count, err, cap) ||
        (transport && !dc_gpu_prepare_water_transport(gpu, occupancy, thermal, err, cap)) ||
        (publish && !prepare_publication(gpu, peer, thermal, err, cap)))
        return DC_LIQUID_SEAM_REJECTED;
    if (!fr_renderer_begin(gpu)) {
        error(err, cap, "Cannot begin liquid seam projection");
        return DC_LIQUID_SEAM_REJECTED;
    }

    const struct dc_gpu_seam_projection *scratch = gpu->seam_projection;
    uint32_t *receipt = projection_field(scratch, PUBLICATION_RECEIPT);
    if (publish) *receipt = 0u;
    uint32_t *pages = projection_field(scratch, PAGES), *slots = projection_field(scratch, SLOT_PAGES);
    const uint32_t stride = width / DC_CHUNK_SIDE;
    memset(pages, 0, scratch->size[PAGES]);
    for (uint32_t tile = 0; tile < count; ++tile) {
        const dc_gpu_water_tile_t *location = &domain->locations[tile];
        const uint32_t page = (location->scratch_y / DC_CHUNK_SIDE) * stride + location->scratch_x / DC_CHUNK_SIDE;
        pages[page] = tile + 1u;
        slots[tile] = page;
    }
    if (count < (width / DC_CHUNK_SIDE) * (height / DC_CHUNK_SIDE)) {
        memset(projection_field(scratch, VELOCITY), 0, scratch->size[VELOCITY]);
        memset(projection_field(scratch, OCCUPANCY), 0, scratch->size[OCCUPANCY]);
    }
    if (publish) memcpy(projection_field(scratch, PUBLICATION_TILES), domain->locations,
                        scratch->size[PUBLICATION_TILES]);
    const VkCommandBuffer command = gpu->command;
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    for (uint32_t tile = 0; tile < count; ++tile)
        dc_gpu_copy_projection_tile(command, scratch, domain->tiles[tile], &domain->locations[tile], tile, transport);
    if (plan)
        vkCmdFillBuffer(command, scratch->buffer, scratch->offset[CROSSINGS], scratch->size[CROSSINGS], 0u);
    if (previous_normal) override_normal(gpu, axis, width, previous_normal);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    const uint32_t seam_axis = !plan ? 0u :
        axis == DC_LIQUID_SEAM_X ? DC_WATER_SEAM_HORIZONTAL : DC_WATER_SEAM_VERTICAL;
    if (transport && occupancy)
        dc_gpu_record_water_displacement(gpu, scratch->sets[0], scratch->displacement, width, height, seam_axis);
    if (plan) record_stage_readback(command, scratch, DISPLACEMENT_CROSSINGS, false);
    dc_gpu_record_dense_projection(gpu, scratch->sets, width, height, seconds * 60.f);
    if (transport) {
        dc_gpu_record_water_seam_transport(gpu, scratch->sets[0], scratch->transport, width, height,
                                            seconds * 60.f, occupancy, seam_axis);
        if (plan) record_stage_readback(command, scratch, ADVECTION_CROSSINGS, true);
        dc_gpu_record_water_seam_correction(gpu, scratch->sets[0], scratch->correction,
                                            width, height, count, seconds * 60.f, seam_axis);
    }
    if (publish)
        dc_gpu_record_water_publication(gpu, &scratch->publication, width, height, count, thermal);
    fr_renderer_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT);

    if (publish) {
        dc_liquid_seam_result_t result = dc_gpu_submit_liquid_seam(gpu, peer, receipt, false, err, cap);
        if (result != DC_LIQUID_SEAM_COMMITTED) return result;
    } else if (!fr_renderer_submit(gpu)) {
        error(err, cap, "Liquid seam projection did not complete");
        return DC_LIQUID_SEAM_REJECTED;
    }

    if (plan) read_water_plan(scratch, axis, width, thermal, plan);
    else if (samples) read_face(scratch, axis, width, false, samples);
    return DC_LIQUID_SEAM_COMMITTED;
}

static dc_gpu_water_tile_t tile_location(dc_liquid_seam_endpoint_t tile, uint32_t x, uint32_t y,
                                         uint32_t context) {
    return (dc_gpu_water_tile_t){tile.slot, tile.tile_x * DC_CHUNK_SIDE, tile.tile_y * DC_CHUNK_SIDE,
        tile.gpu->width, x * DC_CHUNK_SIDE, y * DC_CHUNK_SIDE, context};
}

static dc_liquid_seam_result_t run_seam(dc_liquid_seam_endpoint_t first, dc_liquid_seam_endpoint_t second,
    dc_liquid_seam_axis_t axis, float seconds, const float *previous_normal,
    dc_liquid_seam_sample_t *samples, dc_water_seam_plan_t *plan, bool publish, char *err, uint32_t cap) {
    if (!dc_gpu_liquid_seam_mapped(first, second, axis)) {
        error(err, cap, "Invalid liquid seam projection endpoints");
        return DC_LIQUID_SEAM_REJECTED;
    }
    const dc_liquid_seam_endpoint_t endpoints[] = {first, second};
    const bool vertical = axis == DC_LIQUID_SEAM_Y;
    const dc_gpu_water_tile_t locations[] = {tile_location(first, 0, 0, 0),
        tile_location(second, !vertical, vertical, 1)};
    const water_domain_t domain = {first.gpu, second.gpu, endpoints, locations, SEAM_TILES,
        DC_CHUNK_SIDE * (1u + !vertical), DC_CHUNK_SIDE * (1u + vertical)};
    return run_water_domain(&domain, axis, seconds, previous_normal, samples, plan, publish, err, cap);
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

static bool connected_tiles(const dc_gpu_water_tile_t *locations, uint32_t count) {
    bool seen[MAX_WATER_COHORT_TILES] = {true};
    uint32_t queue[MAX_WATER_COHORT_TILES] = {0};
    uint32_t tail = 1;
    for (uint32_t head = 0; head < tail; ++head) {
        const dc_gpu_water_tile_t *a = &locations[queue[head]];
        for (uint32_t tile = 0; tile < count; ++tile) {
            if (seen[tile]) continue;
            const dc_gpu_water_tile_t *b = &locations[tile];
            const uint32_t dx = a->scratch_x > b->scratch_x ? a->scratch_x - b->scratch_x : b->scratch_x - a->scratch_x;
            const uint32_t dy = a->scratch_y > b->scratch_y ? a->scratch_y - b->scratch_y : b->scratch_y - a->scratch_y;
            if (dx + dy != DC_CHUNK_SIDE) continue;
            seen[tile] = true;
            queue[tail++] = tile;
        }
    }
    return tail == count;
}

static bool cohort_domain(const dc_liquid_seam_endpoint_t *tiles, uint32_t count,
                          dc_gpu_water_tile_t locations[MAX_WATER_COHORT_TILES], water_domain_t *domain) {
    if (!tiles || count < 2u || count > MAX_WATER_COHORT_TILES || !tiles[0].gpu) return false;
    dc_gpu_t *gpu = tiles[0].gpu, *peer = NULL;
    dc_chunk_coord_t min = tiles[0].coord, max = min;
    for (uint32_t tile = 0; tile < count; ++tile) {
        const dc_liquid_seam_endpoint_t endpoint = tiles[tile];
        if (!dc_gpu_liquid_endpoint_mapped(endpoint) || !gpu->device_lifetime ||
            endpoint.gpu->device_lifetime != gpu->device_lifetime ||
            (endpoint.gpu->thermal != NULL) != (gpu->thermal != NULL)) return false;
        if (endpoint.gpu != gpu) {
            if (peer && endpoint.gpu != peer) return false;
            peer = endpoint.gpu;
        }
        for (uint32_t previous = 0; previous < tile; ++previous)
            if (tiles[previous].coord.x == endpoint.coord.x && tiles[previous].coord.y == endpoint.coord.y)
                return false;
        if (endpoint.coord.x < min.x) min.x = endpoint.coord.x;
        if (endpoint.coord.y < min.y) min.y = endpoint.coord.y;
        if (endpoint.coord.x > max.x) max.x = endpoint.coord.x;
        if (endpoint.coord.y > max.y) max.y = endpoint.coord.y;
    }
    const uint64_t dx = (uint64_t)max.x - (uint64_t)min.x, dy = (uint64_t)max.y - (uint64_t)min.y;
    if (!peer || dx >= count || dy >= count) return false;
    for (uint32_t tile = 0; tile < count; ++tile)
        locations[tile] = tile_location(tiles[tile],
            (uint32_t)((uint64_t)tiles[tile].coord.x - (uint64_t)min.x),
            (uint32_t)((uint64_t)tiles[tile].coord.y - (uint64_t)min.y), tiles[tile].gpu == peer);
    if (!connected_tiles(locations, count)) return false;
    *domain = (water_domain_t){gpu, peer, tiles, locations, count,
        ((uint32_t)dx + 1u) * DC_CHUNK_SIDE, ((uint32_t)dy + 1u) * DC_CHUNK_SIDE};
    return true;
}

dc_liquid_seam_result_t dc_gpu_execute_water_cohort(const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, float seconds, char *err, uint32_t cap) {
    dc_gpu_water_tile_t locations[MAX_WATER_COHORT_TILES];
    water_domain_t domain;
    if (!cohort_domain(tiles, count, locations, &domain)) {
        error(err, cap, "Water cohort requires distinct connected tiles in two mapped contexts");
        return DC_LIQUID_SEAM_REJECTED;
    }
    return run_water_domain(&domain, DC_LIQUID_SEAM_X, seconds, NULL, NULL, NULL, true, err, cap);
}

dc_liquid_seam_result_t dc_gpu_step_water_cohort(const dc_liquid_seam_endpoint_t *tiles,
    uint32_t count, float seconds, char *err, uint32_t cap) {
    return dc_gpu_execute_water_cohort(tiles, count, seconds, err, cap);
}
