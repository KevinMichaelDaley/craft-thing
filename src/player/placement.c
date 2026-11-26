#include <math.h>
#include <stdio.h>

#include "ferrum/fire/material_profile.h"
#include "ferrum/player/placement.h"
#include "../vulkan/gpu_internal.h"

enum { PLAYER_BLOCK_SIDE = 4u };

static bool placement_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool placement_footprint(dc_gpu_t *gpu, dc_chunk_coord_t chunk,
                                 uint32_t x, uint32_t y, uint32_t size,
                                 bool *available, char *err, uint32_t cap) {
    *available = false;
    uint32_t first_x = x / DC_CHUNK_SIDE, first_y = y / DC_CHUNK_SIDE;
    uint32_t last_x = (x + size - 1u) / DC_CHUNK_SIDE;
    uint32_t last_y = (y + size - 1u) / DC_CHUNK_SIDE;
    if ((first_x != last_x && chunk.x == INT64_MAX) ||
        (first_y != last_y && chunk.y == INT64_MAX))
        return true;
    for (uint32_t ty = first_y; ty <= last_y; ++ty)
        for (uint32_t tx = first_x; tx <= last_x; ++tx) {
            dc_chunk_coord_t coord = {chunk.x + (tx - first_x),
                                      chunk.y + (ty - first_y)};
            uint32_t slot = dc_gpu_find_active_chunk(gpu, coord);
            if (slot == UINT32_MAX || gpu->slot_page[slot] != ty * gpu->page_width + tx)
                return true;
            if (!dc_gpu_copy_chunk_state(gpu, slot, false, false, err, cap))
                return false;
        }
    const uint32_t *pages = gpu->page_mapped;
    const dc_cell_t *cells = gpu->chunk_mapped;
    for (uint32_t py = y; py < y + size; ++py)
        for (uint32_t px = x; px < x + size; ++px) {
            uint32_t page = pages[py / DC_CHUNK_SIDE * gpu->page_width + px / DC_CHUNK_SIDE];
            uint32_t local = py % DC_CHUNK_SIDE * DC_CHUNK_SIDE + px % DC_CHUNK_SIDE;
            const dc_cell_t *cell = &cells[(size_t)(page - 1u) * DC_CHUNK_CELLS + local];
            if (cell->material != DC_MATERIAL_AIR || cell->fluid_mass != 0u)
                return true;
        }
    if (!dc_gpu_set_body_origin(gpu, gpu->body_origin, err, cap)) return false;
    for (uint32_t py = y; py < y + size; ++py)
        for (uint32_t px = x; px < x + size; ++px) {
            uint32_t occupancy;
            if (!dc_gpu_read_occupancy(gpu, px, py, &occupancy, NULL, err, cap))
                return false;
            if (occupancy != 0u) return true;
        }
    *available = true;
    return true;
}

static bool placement_spawn_block(dc_gpu_t *gpu, dc_chunk_coord_t chunk,
                                   uint32_t x, uint32_t y, uint32_t material,
                                   char *err, uint32_t cap) {
    uint32_t id = dc_gpu_next_body_id(gpu);
    if (!id) return placement_error(err, cap, "Rigid block has no free body ID");
    const int32_t side_fp = PLAYER_BLOCK_SIDE * DC_FLUID_FULL;
    dc_gpu_world_body_t body = {.chunk = chunk, .body = {
        .x_fp = (int32_t)(x * DC_FLUID_FULL), .y_fp = (int32_t)(y * DC_FLUID_FULL),
        .width = PLAYER_BLOCK_SIDE, .height = PLAYER_BLOCK_SIDE, .id = id, .active = 1u}};
    dc_gpu_body_shape_t shape = {.count = 4u, .material = material,
        .vertices = {{0, 0}, {side_fp, 0}, {side_fp, side_fp}, {0, side_fp}}};
    return dc_gpu_spawn_convex_body(gpu, body, &shape, err, cap);
}

bool fr_player_place_material(dc_gpu_t *gpu, fr_player_inventory_t *inventory,
                              const fr_player_point_t *aim, uint16_t material,
                              bool rigid, bool *placed, char *err, uint32_t cap) {
    if (!placed) return placement_error(err, cap, "Missing placement result");
    *placed = false;
    if (!gpu || !inventory) return placement_error(err, cap, "Missing placement world or inventory");
    if (!aim || !(aim->x >= 0.f && aim->x < DC_CHUNK_SIDE &&
                  aim->y >= 0.f && aim->y < DC_CHUNK_SIDE)) return true;
    dc_item_kind_t kind = fr_player_inventory_kind(material);
    const fr_reactive_profile_t *profile = fr_reactive_profile(material);
    if (!kind || !profile || !profile->legacy_body_material) return true;
    uint32_t size = rigid ? PLAYER_BLOCK_SIDE : 1u;
    uint64_t cost = (uint64_t)llround((double)profile->density *
                                     size * size * DC_FLUID_FULL);
    if (inventory->mass_fp[kind] < cost) return true;
    uint32_t slot = dc_gpu_find_active_chunk(gpu, aim->chunk);
    if (slot == UINT32_MAX) return true;
    uint32_t tile = gpu->slot_page[slot];
    uint32_t lx = (uint32_t)aim->x, ly = (uint32_t)aim->y;
    uint32_t x = tile % gpu->page_width * DC_CHUNK_SIDE + lx;
    uint32_t y = tile / gpu->page_width * DC_CHUNK_SIDE + ly;
    if (x >= gpu->width || y >= gpu->height ||
        size > gpu->width - x || size > gpu->height - y) return true;
    bool available;
    if (!placement_footprint(gpu, aim->chunk, x, y, size, &available, err, cap))
        return false;
    if (!available) return true;

    bool okay;
    if (rigid) {
        okay = placement_spawn_block(gpu, aim->chunk, lx, ly,
                                      profile->legacy_body_material, err, cap);
        *placed = okay;
    } else okay = dc_gpu_place_terrain_cell(gpu, x, y, material, placed, err, cap);
    if (*placed) inventory->mass_fp[kind] -= cost;
    return okay;
}
