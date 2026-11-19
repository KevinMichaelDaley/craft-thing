#include <math.h>
#include <stdlib.h>

#include "ferrum/player/chew.h"
#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "../../src/vulkan/gpu_internal.h"

enum { CHEW_REACH_CELLS = 12u, CHEW_RATE_PER_SECOND_FP = 300u };

static dc_item_kind_t world_wood_kind(uint16_t material) {
    return material == DC_MATERIAL_WOOD ? DC_ITEM_WOOD :
           material == DC_MATERIAL_DENSE_WOOD ? DC_ITEM_DENSE_WOOD :
           material == DC_MATERIAL_STONE ? DC_ITEM_STONE : 0u;
}

bool fr_player_chew_wood_tick(dc_gpu_t *gpu,
                         fr_player_inventory_t *inventory,
                         fr_player_chew_t *chew, const fr_player_point_t *target,
                         float effort, char *err_buf, uint32_t err_cap) {
    if (!gpu || !inventory || !chew || !target || effort <= 0.f ||
        target->x < 0.f || target->y < 0.f || target->x >= 64.f ||
        target->y >= 64.f)
        return false;
    uint32_t lx = (uint32_t)target->x, ly = (uint32_t)target->y;
    /* Resolve the resident slot that actually holds the target wood: the
     * streaming registry and the brush page table can disagree about which
     * resident slot serves a coordinate, so each candidate is verified by
     * its own contents. */
    dc_chunk_t *probe = calloc(1, sizeof(*probe));
    if (!probe) return false;
    uint32_t slot = UINT32_MAX;
    uint16_t material = 0u;
    uint32_t bite_x = lx, bite_y = ly;
    uint32_t best_distance = UINT32_MAX;
    for (uint32_t candidate = 0u;
         candidate < DC_GPU_CHUNK_SLOTS && slot == UINT32_MAX; candidate++) {
        if (!dc_gpu_download_chunk(gpu, candidate, probe, err_buf, err_cap))
            continue;
        for (int32_t dy = -1; dy <= 1; dy++)
            for (int32_t dx = -1; dx <= 1; dx++) {
                int32_t cx = (int32_t)lx + dx, cy = (int32_t)ly + dy;
                if (cx < 0 || cx > 63 || cy < 0 || cy > 63) continue;
                uint16_t cell = probe->cells[(uint32_t)cy * 64u +
                                             (uint32_t)cx].material;
                if (!world_wood_kind(cell)) continue;
                uint32_t distance = (uint32_t)(dx * dx + dy * dy);
                if (distance < best_distance) {
                    best_distance = distance;
                    material = cell;
                    bite_x = (uint32_t)cx;
                    bite_y = (uint32_t)cy;
                    slot = candidate;
                }
            }
    }
    free(probe);
    if (slot == UINT32_MAX ||
        best_distance > CHEW_REACH_CELLS * CHEW_REACH_CELLS || !material)
        return true;
    dc_item_kind_t kind = world_wood_kind(material);
    const fr_reactive_profile_t *profile = fr_reactive_profile(material);
    if (!profile) return true;
    /* Deterministic work accounting: effort is weighted by the shared
     * physical density of the reachable pocket's occupied amount, carries
     * across ticks and stays cadence independent. */
    fr_chew_target_t pocket = {
        .parcel_id = ((uint64_t)target->chunk.x << 32) ^
                     ((uint64_t)target->chunk.y << 16) ^
                     ((uint64_t)bite_y << 8) ^ bite_x,
        .reachable = true,
        .material = material,
        .dry_mass_fp = (uint32_t)llround(profile->density *
                                         (double)DC_FLUID_FULL),
        .eligible = true,
        .generation = 0u};
    bool bite_ready = false;
    uint64_t delta_ms = (uint64_t)llround((double)effort * 1000. /
                                          (double)CHEW_RATE_PER_SECOND_FP);
    if (!delta_ms) delta_ms = 1u;
    if (!fr_chew_work_advance(chew, &pocket, (uint32_t)DC_FLUID_FULL,
                              delta_ms, true, &bite_ready))
        return false;
    if (!bite_ready) return true;
    /* One localized bite through the ordinary carve pipeline: unsupported
     * wood releases as rigid bodies and the rest of the tree survives.
     * Credit only when the carve consumed the cell, then acknowledge the
     * banked bite so retries cannot duplicate it. */
    if (!dc_gpu_paint_material(gpu,
                               (uint32_t)target->chunk.x * 64u + bite_x,
                               (uint32_t)target->chunk.y * 64u + bite_y,
                               0u, DC_MATERIAL_AIR, err_buf, err_cap))
        return false;
    inventory->mass_fp[kind] += pocket.dry_mass_fp;
    fr_chew_work_committed(chew, &pocket);
    return true;
}
