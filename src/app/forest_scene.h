#ifndef DC_APP_FOREST_SCENE_H
#define DC_APP_FOREST_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "dungeoncraft/gpu.h"

/* Locked forest-bonfire fixture geometry (assets/fixtures/fire_smoke_v1.json). */
enum {
    DC_FOREST_CLEARED_X = 196, DC_FOREST_CLEARED_Y = 176,
    DC_FOREST_CLEARED_W = 76, DC_FOREST_CLEARED_H = 34,
    DC_FOREST_WARMTH_PROBE_X = 249, DC_FOREST_WARMTH_PROBE_Y = 202,
    DC_FOREST_WARMTH_LOW_KX100 = 29815, DC_FOREST_WARMTH_HIGH_KX100 = 31815,
    DC_FOREST_WARMTH_BEGIN_S = 20, DC_FOREST_WARMTH_END_S = 120,
    DC_FOREST_IGNITION_X = 228, DC_FOREST_IGNITION_Y = 204,
    DC_FOREST_IGNITION_RADIUS = 4,
    DC_FOREST_WET_TREE = 3, DC_FOREST_TREE_COUNT = 4,
    DC_FOREST_PIT_X = 214, DC_FOREST_PIT_Y = 199,
    DC_FOREST_PIT_W = 32, DC_FOREST_PIT_H = 11,
    DC_FOREST_BRIDGE_X = 236, DC_FOREST_BRIDGE_Y = 202,
    DC_FOREST_BRIDGE_W = 76, DC_FOREST_BRIDGE_H = 2
};

typedef struct {
    uint64_t pile_fuel_fp, pile_char_fp, bridge_fuel_fp;
    uint32_t pile_cells, ember_cells;
    uint64_t tree_fuel_fp[DC_FOREST_TREE_COUNT];
    uint64_t wet_tree_water_fp;
    uint32_t pile_body;
} dc_forest_scene_t;

/* Origin-id regions shared by the scene builder and its tests. */
enum {
    DC_FOREST_REGION_PILE = 0, DC_FOREST_REGION_TREE_DRY_1 = 1,
    DC_FOREST_REGION_TREE_DRY_2 = 2, DC_FOREST_REGION_TREE_DRY_3 = 3,
    DC_FOREST_REGION_TREE_WET = 4, DC_FOREST_REGION_BRIDGE = 5
};

/** Build the locked forest bonfire in a fully resident 640x448 world with an
 * ignited charcoal starter, painted rigid pile and fixed dry/wet forest.
 * The optional dry bridge or broken stone ring unlocks the containment. */
bool dc_forest_scene_build(dc_gpu_t *gpu, bool dry_bridge, bool broken_ring,
                           dc_forest_scene_t *out, char *err_buf, uint32_t err_cap);

#endif
