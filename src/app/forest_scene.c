#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forest_scene.h"

#include "dungeoncraft/color.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "smoke_profile.h"

#ifdef DC_FOREST_PRESENTATION
#include "ferrum/renderer/gpu.h"
#endif

enum {DC_FOREST_TILES_X = 10, DC_FOREST_TILES_Y = 7, DC_FOREST_TILE_COUNT = 70,
      /* Per-object connected clusters stay inside the 16-cell body side limit. */
      DC_FOREST_OBJECT_PILE = 1u, DC_FOREST_OBJECT_STICK_LEFT = 2u,
      DC_FOREST_OBJECT_STICK_RIGHT = 3u};

#define DC_FOREST_OBJECT_TRANSACTION UINT64_C(0x464f520000000001)

typedef struct {
    uint32_t x, y, width, height;
    uint16_t material;
    uint32_t flags;
} forest_rect_t;

static const forest_rect_t forest_ground[] = {
    {0u, 210u, 480u, 60u, DC_MATERIAL_STONE, DC_CELL_LOADED_TERRAIN},
    {214u, 207u, 32u, 3u, DC_MATERIAL_STONE, DC_CELL_LOADED_TERRAIN},
    {214u, 199u, 3u, 11u, DC_MATERIAL_STONE, DC_CELL_LOADED_TERRAIN},
    {243u, 199u, 3u, 11u, DC_MATERIAL_STONE, DC_CELL_LOADED_TERRAIN}};
static const forest_rect_t forest_trees[][2] = {
    {{88u, 154u, 4u, 56u, DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN},
     {90u, 150u, 16u, 0u, DC_MATERIAL_LEAVES, DC_CELL_LOADED_TERRAIN}},
    {{143u, 160u, 4u, 50u, DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN},
     {145u, 156u, 16u, 0u, DC_MATERIAL_LEAVES, DC_CELL_LOADED_TERRAIN}},
    {{308u, 158u, 4u, 52u, DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN},
     {310u, 154u, 16u, 0u, DC_MATERIAL_LEAVES, DC_CELL_LOADED_TERRAIN}},
    {{368u, 154u, 4u, 56u, DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN},
     {370u, 150u, 16u, 0u, DC_MATERIAL_LEAVES, DC_CELL_LOADED_TERRAIN}}};
static const forest_rect_t forest_pile[] = {
    {220u, 203u, 16u, 4u, DC_MATERIAL_WOOD, 0u},
    {224u, 200u, 1u, 3u, DC_MATERIAL_WOOD, 0u},
    {232u, 200u, 1u, 3u, DC_MATERIAL_WOOD, 0u}};
static const forest_rect_t forest_stick_left = {218u, 202u, 12u, 1u, DC_MATERIAL_WOOD, 0u};
static const forest_rect_t forest_stick_right = {230u, 202u, 10u, 1u, DC_MATERIAL_WOOD, 0u};
static const forest_rect_t forest_bridge = {236u, 202u, 76u, 2u, DC_MATERIAL_WOOD,
                                            DC_CELL_LOADED_TERRAIN};

static bool forest_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static bool forest_in_rect(uint32_t x, uint32_t y, const forest_rect_t *rect) {
    if (rect->height == 0u) {
        int32_t dx = (int32_t)x - (int32_t)rect->x;
        int32_t dy = (int32_t)y - (int32_t)rect->y;
        int32_t radius = (int32_t)rect->width;
        return dx * dx + dy * dy <= radius * radius;
    }
    return x >= rect->x && x < rect->x + rect->width &&
           y >= rect->y && y < rect->y + rect->height;
}

static uint32_t forest_pile_object(uint32_t x, uint32_t y) {
    for (uint32_t rect = 0; rect < 3u; rect++)
        if (forest_in_rect(x, y, &forest_pile[rect])) return DC_FOREST_OBJECT_PILE;
    if (forest_in_rect(x, y, &forest_stick_left)) return DC_FOREST_OBJECT_STICK_LEFT;
    if (forest_in_rect(x, y, &forest_stick_right)) return DC_FOREST_OBJECT_STICK_RIGHT;
    return 0u;
}

static bool forest_ignition_cell(uint32_t x, uint32_t y) {
    int32_t dx = (int32_t)x - DC_FOREST_IGNITION_X;
    int32_t dy = (int32_t)y - DC_FOREST_IGNITION_Y;
    return dx * dx + dy * dy <= DC_FOREST_IGNITION_RADIUS * DC_FOREST_IGNITION_RADIUS;
}

static uint64_t forest_origin(uint32_t region, uint32_t x, uint32_t y) {
    return ((uint64_t)region << 58) | ((uint64_t)y << 20) | x;
}

static void forest_fill_rect(const forest_rect_t *rect, bool broken_ring,
                             dc_chunk_t *chunks) {
    uint32_t start_y = rect->height ? rect->y : rect->y - rect->width;
    uint32_t end_y = rect->height ? rect->y + rect->height
                                  : rect->y + rect->width + 1u;
    for (uint32_t y = start_y; y < end_y && y < 448u; y++)
        for (uint32_t x = 0; x < 640u; x++) {
            if (!forest_in_rect(x, y, rect)) continue;
            if (broken_ring && x >= 243u && x < 246u && y >= 199u && y < 207u)
                continue;
            uint32_t tile = (y / 64u) * DC_FOREST_TILES_X + x / 64u;
            dc_cell_t *cell = &chunks[tile].cells[(y % 64u) * 64u + x % 64u];
            cell->material = rect->material;
            cell->flags = rect->flags;
        }
}

static uint32_t forest_starter_cells(const forest_rect_t *const *rects,
                                     uint32_t rect_count) {
    uint32_t count = 0;
    for (uint32_t y = DC_FOREST_IGNITION_Y - DC_FOREST_IGNITION_RADIUS;
         y <= DC_FOREST_IGNITION_Y + DC_FOREST_IGNITION_RADIUS; y++)
        for (uint32_t x = DC_FOREST_IGNITION_X - DC_FOREST_IGNITION_RADIUS;
             x <= DC_FOREST_IGNITION_X + DC_FOREST_IGNITION_RADIUS; x++)
            for (uint32_t index = 0; index < rect_count; index++)
                if (forest_in_rect(x, y, rects[index]) &&
                    forest_ignition_cell(x, y)) {
                    count++;
                    break;
                }
    return count;
}

/* Seed one origin region across a sorted union of rectangles/circles. */
static bool forest_seed_region(const dc_chunk_t *chunks, const forest_rect_t *rects,
                               uint32_t rect_count, uint32_t region, bool wet,
                               bool starter, float starter_share, uint64_t *fuel,
                               uint64_t *water_total, uint64_t *char_total,
                               uint32_t *embers, uint64_t *parcel_counter,
                               fr_reactive_chunk_t *packets, char *err, uint32_t cap) {
    uint32_t start_y = 448u, end_y = 0u;
    for (uint32_t index = 0; index < rect_count; index++) {
        const forest_rect_t *rect = &rects[index];
        uint32_t low = rect->height ? rect->y : rect->y - rect->width;
        uint32_t high = rect->height ? rect->y + rect->height
                                     : rect->y + rect->width + 1u;
        if (low < start_y) start_y = low;
        if (high > end_y) end_y = high;
    }
    for (uint32_t y = start_y; y < end_y && y < 448u; y++)
        for (uint32_t x = 0; x < 640u; x++) {
            int32_t match = -1;
            for (uint32_t index = 0; index < rect_count; index++)
                if (forest_in_rect(x, y, &rects[index])) match = (int32_t)index;
            if (match < 0) continue;
            const forest_rect_t *rect = &rects[match];
            const fr_reactive_profile_t *material_profile =
                fr_reactive_profile(rect->material);
            if (!material_profile)
                return forest_error(err, cap, "Missing forest material profile");
            uint32_t mass = (uint32_t)llround(material_profile->density * 65536.);
            uint32_t tile = (y / 64u) * DC_FOREST_TILES_X + x / 64u;
            uint32_t local = (y % 64u) * 64u + x % 64u;
            uint32_t water = 0;
            if (wet)
                water = (uint32_t)llround((double)mass *
                                          material_profile->moisture_capacity * .95);
            fr_reactive_state_t state;
            uint64_t parcel = (*parcel_counter)++;
            if (!fr_reactive_state_default(rect->material, mass, water, 293.15f,
                                           parcel, &state, err, cap))
                return false;
            uint64_t origin = forest_origin(region, x, y);
            state.origin_id_low = (uint32_t)origin;
            state.origin_id_high = (uint32_t)(origin >> 32);
            /* Locked ignition input: the starter cells carry the ignition flag
             * and share 12000 finite energy units, not an external reservoir. */
            if (starter && forest_ignition_cell(x, y) && starter_share > 0.) {
                state.char_mass_fp = state.fuel_mass_fp;
                state.fuel_mass_fp = 0;
                if (!fr_reactive_state_add_heat(&state, starter_share, err, cap))
                    return false;
                state.flags |= FR_REACTIVE_FLAMING;
                if (embers) (*embers)++;
            }
            fr_reactive_chunk_t *packet = &packets[tile];
            fr_reactive_attachment_t *attachment =
                realloc(packet->attachments,
                        (size_t)(packet->count + 1u) * sizeof(*attachment));
            if (!attachment) return forest_error(err, cap, "Cannot stage forest parcel");
            packet->attachments = attachment;
            fr_reactive_attachment_t *entry =
                &packet->attachments[packet->count++];
            entry->local_index = local;
            entry->record.owner =
                fr_reactive_cell_owner(chunks[tile].coord, local);
            entry->record.state = state;
            *fuel += state.fuel_mass_fp + state.char_mass_fp;
            *char_total += state.char_mass_fp;
            *water_total += water;
        }
    return true;
}

bool dc_forest_scene_build(dc_gpu_t *gpu, bool dry_bridge, bool broken_ring,
                           dc_forest_scene_t *out, char *err_buf, uint32_t err_cap) {
    if (!gpu) return forest_error(err_buf, err_cap, "Missing forest GPU context");
    dc_chunk_t *chunks = calloc(DC_FOREST_TILE_COUNT, sizeof(*chunks));
    dc_chunk_colors_t *colors = calloc(DC_FOREST_TILE_COUNT, sizeof(*colors));
    fr_reactive_chunk_t *packets = calloc(DC_FOREST_TILE_COUNT, sizeof(*packets));
    uint32_t *objects = calloc(DC_GPU_CHUNK_SLOTS * 4096u, sizeof(*objects));
    if (!chunks || !colors || !packets || !objects) {
        free(chunks);
        free(colors);
        free(packets);
        free(objects);
        return forest_error(err_buf, err_cap, "Cannot allocate forest staging");
    }
    for (uint32_t tile = 0; tile < DC_FOREST_TILE_COUNT; tile++)
        chunks[tile].coord = (dc_chunk_coord_t){tile % DC_FOREST_TILES_X,
                                                tile / DC_FOREST_TILES_X};
    for (uint32_t rect = 0; rect < 4u; rect++)
        forest_fill_rect(&forest_ground[rect], broken_ring, chunks);
    for (uint32_t tree = 0; tree < DC_FOREST_TREE_COUNT; tree++)
        for (uint32_t part = 0; part < 2u; part++)
            forest_fill_rect(&forest_trees[tree][part], broken_ring, chunks);
    for (uint32_t rect = 0; rect < 3u; rect++)
        forest_fill_rect(&forest_pile[rect], false, chunks);
    forest_fill_rect(&forest_stick_left, false, chunks);
    forest_fill_rect(&forest_stick_right, false, chunks);
    if (dry_bridge)
        /* The dry bridge lies across the ring without replacing pile cells. */
        for (uint32_t y = forest_bridge.y;
             y < forest_bridge.y + forest_bridge.height; y++)
            for (uint32_t x = forest_bridge.x;
                 x < forest_bridge.x + forest_bridge.width; x++) {
                if (forest_pile_object(x, y)) continue;
                uint32_t tile = (y / 64u) * DC_FOREST_TILES_X + x / 64u;
                dc_cell_t *cell = &chunks[tile].cells[(y % 64u) * 64u + x % 64u];
                cell->material = forest_bridge.material;
                cell->flags = forest_bridge.flags;
            }
    for (uint32_t tile = 0; tile < DC_FOREST_TILE_COUNT; tile++)
        for (uint32_t i = 0; i < 4096u; i++) {
            uint32_t x = chunks[tile].coord.x * 64u + i % 64u;
            uint32_t y = chunks[tile].coord.y * 64u + i / 64u;
            uint32_t pile_object = forest_pile_object(x, y);
            if (pile_object) objects[tile * 4096u + i] = pile_object;
            float grain = (float)(((uint64_t)x * 3u + (uint64_t)y * 11u) & 15u) / 255.f;
            float *color = colors[tile].terrain[i];
            if (pile_object) {
                color[0] = .38f;
                color[1] = .24f;
                color[2] = .10f;
            } else {
                uint32_t material = chunks[tile].cells[i].material;
                if (material == DC_MATERIAL_WOOD || material == DC_MATERIAL_LEAVES) {
                    color[0] = material == DC_MATERIAL_LEAVES ? .18f : .35f;
                    color[1] = material == DC_MATERIAL_LEAVES ? .42f : .22f;
                    color[2] = material == DC_MATERIAL_LEAVES ? .12f : .10f;
                } else if (material == DC_MATERIAL_STONE) {
                    color[0] = .23f;
                    color[1] = .25f;
                    color[2] = .28f;
                } else {
                    color[0] = .07f;
                    color[1] = .07f;
                    color[2] = .09f;
                }
            }
            color[0] += grain;
            color[1] += grain;
            color[2] += grain;
            color[3] = 1.f;
        }
    dc_forest_scene_t scene = {0};
    uint64_t ignored = 0;
    uint32_t ignored_cells = 0;
    /* Parcel ids stay above every possible encoded origin so the runtime
     * high-water mark never rejects a staged record. */
    uint64_t parcel_counter = 6ull << 58;
    const forest_rect_t pile_regions[5] = {forest_pile[0], forest_pile[1],
                                           forest_pile[2], forest_stick_left,
                                           forest_stick_right};
    const forest_rect_t *pile_pointers[5] = {&pile_regions[0], &pile_regions[1],
                                             &pile_regions[2], &pile_regions[3],
                                             &pile_regions[4]};
    uint32_t starter_share_cells = forest_starter_cells(pile_pointers, 5u);
    float starter_share = starter_share_cells
                              ? 12000.f / (float)starter_share_cells
                              : 0.f;
    bool okay = forest_seed_region(chunks, pile_regions, 5u, DC_FOREST_REGION_PILE,
                                   false, true, starter_share, &scene.pile_fuel_fp,
                                   &ignored, &scene.pile_char_fp, &scene.ember_cells,
                                   &parcel_counter, packets, err_buf, err_cap);
    scene.pile_cells = 64u + 6u + 12u + 10u;
    for (uint32_t tree = 0; okay && tree < DC_FOREST_TREE_COUNT; tree++) {
        uint32_t region = tree < DC_FOREST_WET_TREE
                              ? DC_FOREST_REGION_TREE_DRY_1 + tree
                              : DC_FOREST_REGION_TREE_WET;
        bool wet = tree == DC_FOREST_WET_TREE;
        uint64_t water = 0, fuel = 0;
        okay = forest_seed_region(chunks, forest_trees[tree], 2u, region, wet,
                                  false, 0.f, &fuel, &water, &ignored,
                                  &ignored_cells, &parcel_counter, packets,
                                  err_buf, err_cap);
        scene.tree_fuel_fp[tree] += fuel;
        if (wet) scene.wet_tree_water_fp += water;
    }
    if (okay && dry_bridge) {
        /* Staged bridge fuel stops short of the tree trunk cells and skips
         * the cells the pile objects already own. */
        const forest_rect_t bridge_regions[2] = {{240u, 202u, 68u, 2u,
                                                  DC_MATERIAL_WOOD,
                                                  DC_CELL_LOADED_TERRAIN},
                                                 {236u, 203u, 4u, 1u,
                                                  DC_MATERIAL_WOOD,
                                                  DC_CELL_LOADED_TERRAIN}};
        okay = forest_seed_region(chunks, bridge_regions, 2u,
                                  DC_FOREST_REGION_BRIDGE, false, false, 0.f,
                                  &scene.bridge_fuel_fp, &ignored, &ignored,
                                  &ignored_cells, &parcel_counter, packets,
                                  err_buf, err_cap);
    }
    uint32_t total = 0;
    for (uint32_t tile = 0; tile < DC_FOREST_TILE_COUNT; tile++)
        total += packets[tile].count;
    if (okay && !fr_gpu_reactive_reserve_import(gpu, total, err_buf, err_cap))
        okay = false;
    for (uint32_t tile = 0; okay && tile < DC_FOREST_TILE_COUNT; tile++) {
        packets[tile].next_id = parcel_counter;
        if (!packets[tile].count) continue;
        okay = dc_gpu_upload_chunk(gpu, tile, &chunks[tile], err_buf, err_cap) &&
               dc_gpu_set_page(gpu, tile % DC_FOREST_TILES_X,
                               tile / DC_FOREST_TILES_X, tile, err_buf, err_cap) &&
               fr_gpu_reactive_import_chunk(gpu, tile, &chunks[tile],
                                            &packets[tile], err_buf, err_cap);
        if (!okay && getenv("DC_FOREST_TRACE")) {
            printf("import fail tile %u count %u: %s\n", tile, packets[tile].count,
                   err_buf);
            for (uint32_t i = 0; i < packets[tile].count && i < 4u; i++) {
                fr_reactive_attachment_t *a = &packets[tile].attachments[i];
                printf("  entry %u local %u material %u cellmat %u\n", i,
                       a->local_index, a->record.state.material,
                       chunks[tile].cells[a->local_index].material);
            }
        }
    }
    for (uint32_t tile = 0; tile < DC_FOREST_TILE_COUNT; tile++)
        if (!packets[tile].count || !okay)
            fr_reactive_chunk_release(&packets[tile]);
    for (uint32_t tile = 0; okay && tile < DC_FOREST_TILE_COUNT; tile++) {
        if (packets[tile].count) continue;
        okay = dc_gpu_upload_chunk(gpu, tile, &chunks[tile], err_buf, err_cap) &&
               dc_gpu_set_page(gpu, tile % DC_FOREST_TILES_X,
                               tile / DC_FOREST_TILES_X, tile, err_buf, err_cap);
    }
    for (uint32_t tile = 0; tile < DC_FOREST_TILE_COUNT; tile++)
        fr_reactive_chunk_release(&packets[tile]);
    free(packets);
    packets = NULL;
    free(chunks);
    chunks = NULL;
    if (okay)
        okay = fr_thermal_gpu_enable(gpu, err_buf, err_cap) &&
               dc_gpu_set_rigid_solver(gpu, true) &&
               fr_gpu_admit_rigid_objects(gpu, DC_FOREST_OBJECT_TRANSACTION,
                                          objects, DC_GPU_CHUNK_SLOTS * 4096u,
                                          err_buf, err_cap);
    free(objects);
    objects = NULL;
    for (uint32_t tick = 0; okay && tick < 5u; tick++)
        okay = dc_gpu_rigid_step(gpu, err_buf, err_cap);
    if (okay) {
        float best = -1.f;
        for (uint32_t probe = 0x80000001u; probe <= 0x80000008u; probe++) {
            dc_gpu_world_body_t body;
            if (!dc_gpu_read_world_body(gpu, probe, &body, err_buf, err_cap))
                continue;
            float x = (float)body.chunk.x * 64.f + body.body.x_fp / 65536.f;
            float y = (float)body.chunk.y * 64.f + body.body.y_fp / 65536.f;
            if (getenv("DC_FOREST_TRACE"))
                printf("probe id %x pos %.1f %.1f size %u %u\n", probe, x, y,
                       body.body.width, body.body.height);
            if (x >= 219.f && x <= 236.f && y >= 199.f && y <= 201.f) {
                float distance = fabsf(x - 220.f) + fabsf(y - 200.f);
                if (best < 0.f || distance < best) {
                    best = distance;
                    scene.pile_body = probe;
                }
            }
        }
        okay = scene.pile_body != 0u;
        if (!okay) forest_error(err_buf, err_cap, "Forest pile body was not admitted");
    }
    if (okay && !getenv("DC_FOREST_NO_GAS")) {
        okay = dc_smoke_profile_defaults() &&
               fr_gas_gpu_enable(gpu, &(fr_gas_model_t){
                                          .gas_constant = {.25984, .29680, .18892, .46152, 0},
                                          .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
                                          .kinetic_scale = 1. / 512.},
                                 .0012f, 293.15f, err_buf, err_cap) &&
               fr_gas_gpu_set_pressure_policy(gpu, FR_GAS_PRESSURE_PRACTICAL,
                                              err_buf, err_cap) &&
               fr_gas_gpu_set_gravity(gpu, 900.f, err_buf, err_cap) &&
               (getenv("DC_FOREST_NO_COMBUSTION") ||
                fr_combustion_gpu_enable(gpu, err_buf, err_cap));
    }
#ifdef DC_FOREST_PRESENTATION
    if (okay) {
        okay = fr_renderer_enable(gpu, err_buf, err_cap);
        for (uint32_t tile = 0; okay && tile < DC_FOREST_TILE_COUNT; tile++)
            okay = fr_renderer_upload_chunk_colors(gpu, tile, &colors[tile],
                                                   err_buf, err_cap);
        if (okay) okay = fr_renderer_set_mode(gpu, FR_RENDERER_ALBEDO);
    }
#endif
    free(colors);
    if (!okay) return NULL;
    if (out) *out = scene;
    return gpu;
}
