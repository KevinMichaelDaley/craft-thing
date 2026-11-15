#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/player/chew.h"
#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/fire/material_profile.h"

enum { CHEW_REACH_CELLS = 12u, CHEW_RATE_PER_SECOND_FP = 300u };

void fr_player_inventory_init(fr_player_inventory_t *inventory) {
    if (!inventory) return;
    memset(inventory, 0, sizeof(*inventory));
}

uint64_t fr_player_inventory_mass(const fr_player_inventory_t *inventory,
                                  dc_item_kind_t kind) {
    if (!inventory || kind == 0u || kind >= 8u) return 0u;
    return inventory->mass_fp[kind];
}

static dc_item_kind_t wood_kind(uint16_t material) {
    return material == DC_MATERIAL_WOOD ? DC_ITEM_WOOD :
           material == DC_MATERIAL_DENSE_WOOD ? DC_ITEM_DENSE_WOOD : 0u;
}

static double item_density(dc_item_kind_t kind) {
    return kind == DC_ITEM_WOOD ? DC_DENSITY_WOOD :
           kind == DC_ITEM_DENSE_WOOD ? DC_DENSITY_DENSE_WOOD : 0.;
}

bool fr_player_chew_wood_tick(dc_gpu_t *gpu,
                         fr_player_inventory_t *inventory,
                         fr_player_chew_t *chew, const fr_player_point_t *target,
                         float effort, char *err_buf, uint32_t err_cap) {
    if (!gpu || !inventory || !chew || !target || effort <= 0.f ||
        target->x < 0.f || target->y < 0.f || target->x >= 64.f ||
        target->y >= 64.f) {
        return false;
    }
    uint32_t lx = (uint32_t)target->x, ly = (uint32_t)target->y;
    /* Resolve the resident slot that actually holds the target wood: the
     * streaming registry and the brush page table can disagree about which
     * resident slot serves a coordinate, so each candidate is verified by
     * its own contents. */
    dc_chunk_t *probe = calloc(1, sizeof(*probe));
    if (!probe) return false;
    uint32_t slot = UINT32_MAX;
    bool okay = true;
    for (uint32_t candidate = 0u;
         candidate < DC_GPU_CHUNK_SLOTS && slot == UINT32_MAX; candidate++) {
        if (!dc_gpu_download_chunk(gpu, candidate, probe, err_buf, err_cap))
            continue;
        for (int32_t dy = -1; dy <= 1; dy++)
            for (int32_t dx = -1; dx <= 1; dx++) {
                int32_t cx = (int32_t)lx + dx, cy = (int32_t)ly + dy;
                if (cx < 0 || cx > 63 || cy < 0 || cy > 63) continue;
                if (wood_kind(probe->cells[(uint32_t)cy * 64u +
                                           (uint32_t)cx].material)) {
                    slot = candidate;
                    break;
                }
            }
    }
    if (slot == UINT32_MAX) {
        free(probe);
        return true;
    }
    uint16_t material = 0u;
    uint32_t bite_x = lx, bite_y = ly;
    uint32_t best_distance = UINT32_MAX;
    if (okay)
        for (int32_t dy = -1; dy <= 1 && okay; dy++)
            for (int32_t dx = -1; dx <= 1 && okay; dx++) {
                int32_t cx = (int32_t)lx + dx, cy = (int32_t)ly + dy;
                if (cx < 0 || cx > 63 || cy < 0 || cy > 63) continue;
                uint16_t cell = probe->cells[(uint32_t)cy * 64u +
                                             (uint32_t)cx].material;
                if (!wood_kind(cell)) continue;
                uint32_t distance = (uint32_t)(dx * dx + dy * dy);
                if (distance < best_distance) {
                    best_distance = distance;
                    material = cell;
                    bite_x = (uint32_t)cx;
                    bite_y = (uint32_t)cy;
                }
            }
    free(probe);
    if (best_distance > CHEW_REACH_CELLS * CHEW_REACH_CELLS || !material)
        return true;
    dc_item_kind_t kind = wood_kind(material);
    /* Accumulated work is inverse to the shared physical density: denser
     * wood takes longer at the same chewing effort, and partial progress
     * carries across ticks so equal elapsed effort removes equal mass. */
    chew->progress += effort / ((float)item_density(kind) *
                                (float)DC_FLUID_FULL /
                                (float)CHEW_RATE_PER_SECOND_FP);
    /* Localized bites through the ordinary carve pipeline: unsupported
     * wood releases as rigid bodies and the rest of the tree survives.
     * Credit each bite only after its carve actually consumed the cell.
     * Completed bites all fire within the same tick so equal elapsed
     * effort removes equal mass at any frame cadence. */
    uint32_t bites = 0u;
    while (chew->progress >= 1.f && bites < 32u) {
        chew->progress -= 1.f;
        if (!dc_gpu_paint_material(gpu,
                                   (uint32_t)target->chunk.x * 64u + bite_x,
                                   (uint32_t)target->chunk.y * 64u + bite_y,
                                   0u, DC_MATERIAL_AIR, err_buf, err_cap)) {
            return false;
        }
        inventory->mass_fp[kind] +=
            (uint64_t)lround(item_density(kind) * (double)DC_FLUID_FULL);
        bites++;
    }
    (void)bites;
    return true;
}
