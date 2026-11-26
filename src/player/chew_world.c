#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/player/chew.h"
#include "../vulkan/gpu_internal.h"

enum { HARVEST_REACH_CELLS = 12, HARVEST_PROBE_CHUNKS = 4 };

typedef struct {
    fr_player_point_t point;
    uint32_t slot, tile, local;
    uint16_t material;
} harvest_cell_t;

static bool canonical_point(const fr_player_point_t *point) {
    return point && point->x >= 0.f && point->x < DC_CHUNK_SIDE &&
                    point->y >= 0.f && point->y < DC_CHUNK_SIDE;
}

static bool offset_point(const fr_player_point_t *aim, int32_t dx, int32_t dy,
                         fr_player_point_t *point) {
    dc_chunk_coord_t offset;
    uint32_t x, y;
    dc_cell_address((int64_t)aim->x + dx, (int64_t)aim->y + dy, &offset, &x, &y);
    if ((offset.x < 0 && aim->chunk.x == INT64_MIN) ||
        (offset.x > 0 && aim->chunk.x == INT64_MAX) ||
        (offset.y < 0 && aim->chunk.y == INT64_MIN) ||
        (offset.y > 0 && aim->chunk.y == INT64_MAX))
        return false;
    *point = (fr_player_point_t){{aim->chunk.x + offset.x, aim->chunk.y + offset.y},
                                 (float)x, (float)y};
    return true;
}

static double chunk_distance(int64_t target, int64_t origin) {
    return target >= origin
        ? (double)((uint64_t)target - (uint64_t)origin)
        : -(double)((uint64_t)origin - (uint64_t)target);
}

static bool within_reach(const fr_player_point_t *origin,
                         const fr_player_point_t *point) {
    double x = chunk_distance(point->chunk.x, origin->chunk.x) * DC_CHUNK_SIDE +
               point->x - origin->x;
    double y = chunk_distance(point->chunk.y, origin->chunk.y) * DC_CHUNK_SIDE +
               point->y - origin->y;
    return x * x + y * y <= HARVEST_REACH_CELLS * HARVEST_REACH_CELLS;
}

static bool harvest_target(dc_gpu_t *gpu, const fr_player_point_t *origin,
                           const fr_player_point_t *aim, harvest_cell_t *cell,
                           char *err, uint32_t cap) {
    static const int8_t neighbors[][2] = {
        {0, 0}, {0, -1}, {-1, 0}, {1, 0}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    };
    uint32_t read_slots[HARVEST_PROBE_CHUNKS], read_count = 0u;
    if (dc_gpu_find_active_chunk(gpu, aim->chunk) == UINT32_MAX) return true;

    for (size_t i = 0; i < sizeof(neighbors) / sizeof(neighbors[0]); ++i) {
        fr_player_point_t point;
        if (!offset_point(aim, neighbors[i][0], neighbors[i][1], &point) ||
            !within_reach(origin, &point))
            continue;
        uint32_t slot = dc_gpu_find_active_chunk(gpu, point.chunk);
        if (slot == UINT32_MAX) continue;
        uint32_t tile = gpu->slot_page[slot];
        uint32_t x = tile % gpu->page_width * DC_CHUNK_SIDE + (uint32_t)point.x;
        uint32_t y = tile / gpu->page_width * DC_CHUNK_SIDE + (uint32_t)point.y;
        if (x >= gpu->width || y >= gpu->height) continue;

        uint32_t read = 0;
        while (read < read_count && read_slots[read] != slot) ++read;
        if (read == read_count) {
            if (!dc_gpu_copy_chunk_state(gpu, slot, false, false, err, cap))
                return false;
            read_slots[read_count++] = slot;
        }
        uint32_t local = (uint32_t)point.y * DC_CHUNK_SIDE + (uint32_t)point.x;
        const dc_cell_t *cells = gpu->chunk_mapped;
        uint16_t material = cells[(size_t)slot * DC_CHUNK_CELLS + local].material;
        if (!fr_player_inventory_kind(material)) continue;
        *cell = (harvest_cell_t){point, slot, tile, local, material};
        break;
    }
    return true;
}

static bool harvest_mass(dc_gpu_t *gpu, const harvest_cell_t *cell,
                         fr_chew_target_t *target, char *err, uint32_t cap) {
    fr_reactive_record_t parcel;
    bool found;
    if (!fr_gpu_reactive_read_cell(gpu, cell->slot, cell->local, &parcel, &found,
                                   err, cap))
        return false;
    const uint32_t *owner = dc_gpu_structural_data(gpu) +
                           DC_STRUCTURAL_HEADER_WORDS + cell->slot * 8u;
    uint64_t id = 0;
    uint64_t dry_mass;
    if (found) {
        fr_reactive_owner_t expected = fr_reactive_cell_owner(cell->point.chunk,
                                                               cell->local);
        if (parcel.state.material != cell->material ||
            memcmp(&expected, &parcel.owner, sizeof(expected)) != 0)
            return true;
        id = (uint64_t)parcel.state.parcel_id_high << 32 | parcel.state.parcel_id_low;
        dry_mass = (uint64_t)parcel.state.fuel_mass_fp + parcel.state.char_mass_fp +
                   parcel.state.inert_mass_fp;
    } else {
        const fr_reactive_profile_t *profile = fr_reactive_profile(cell->material);
        if (!profile) return true;
        dry_mass = (uint64_t)lroundf(profile->density * DC_FLUID_FULL);
    }
    if (dry_mass > 0u && dry_mass <= UINT32_MAX) {
        *target = (fr_chew_target_t){.parcel_id = id, .reachable = true,
            .material = cell->material, .dry_mass_fp = (uint32_t)dry_mass,
            .eligible = true, .generation = owner[4]};
    }
    return true;
}

bool fr_player_harvest_tick(dc_gpu_t *gpu, fr_player_inventory_t *inventory,
    fr_player_chew_t *chew, const fr_player_point_t *origin,
    const fr_player_point_t *aim, bool held, uint64_t elapsed_ms,
    bool *eligible, char *err, uint32_t cap) {
    if (!gpu || !inventory || !chew || !eligible) return false;
    *eligible = false;
    if (!canonical_point(origin) || !canonical_point(aim)) {
        *chew = (fr_player_chew_t){0};
        return true;
    }

    harvest_cell_t cell = {0};
    fr_chew_target_t target = {0};
    if (!harvest_target(gpu, origin, aim, &cell, err, cap) ||
        (cell.material && !harvest_mass(gpu, &cell, &target, err, cap)))
        return false;
    if (!target.eligible) {
        *chew = (fr_player_chew_t){0};
        return true;
    }
    *eligible = true;
    if (chew->chunk.x != cell.point.chunk.x || chew->chunk.y != cell.point.chunk.y ||
        chew->slot != cell.slot || chew->local != cell.local) {
        *chew = (fr_player_chew_t){.chunk = cell.point.chunk,
                                  .slot = cell.slot, .local = cell.local};
    }
    bool ready;
    if (!fr_chew_work_advance(&chew->bank, &target, DC_FLUID_FULL, elapsed_ms,
                              held, &ready))
        return false;
    dc_item_kind_t kind = fr_player_inventory_kind(cell.material);
    if (!held || !ready || inventory->mass_fp[kind] > UINT64_MAX - target.dry_mass_fp)
        return true;

    uint32_t x = cell.tile % gpu->page_width * DC_CHUNK_SIDE + (uint32_t)cell.point.x;
    uint32_t y = cell.tile / gpu->page_width * DC_CHUNK_SIDE + (uint32_t)cell.point.y;
    bool removed;
    bool okay = dc_gpu_carve_terrain_cell(gpu, x, y, &removed, err, cap);
    if (removed) {
        inventory->mass_fp[kind] += target.dry_mass_fp;
        fr_chew_work_committed(&chew->bank, &target);
    }
    return okay;
}
