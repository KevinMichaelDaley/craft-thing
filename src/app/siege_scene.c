#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "siege_scene.h"

#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/physics/authored_rigid_gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "smoke_profile.h"

#ifdef DC_SIEGE_PRESENTATION
#include "ferrum/renderer/gpu.h"
#endif

enum {DC_SIEGE_TILES_X = 10, DC_SIEGE_TILES_Y = 7, DC_SIEGE_TILE_COUNT = 70,
      DC_SIEGE_OBJECT_COAL = 1u, DC_SIEGE_OBJECT_ARM = 2u,
      DC_SIEGE_FLOOR_Y = 230u, DC_SIEGE_BASE_X = 76u, DC_SIEGE_BASE_Y = 222u,
      DC_SIEGE_BASE_W = 48u, DC_SIEGE_BASE_H = 8u,
      DC_SIEGE_POST_LEFT_X = 84u, DC_SIEGE_POST_RIGHT_X = 116u,
      DC_SIEGE_POST_Y = 198u, DC_SIEGE_POST_W = 4u, DC_SIEGE_POST_H = 24u,
      DC_SIEGE_BAR_X = 84u, DC_SIEGE_BAR_Y = 188u,
      DC_SIEGE_BAR_W = 36u, DC_SIEGE_BAR_H = 4u,
      /* Cocked release pose; the pivot spring is calibrated, not locked. */
      DC_SIEGE_SPRING_STIFFNESS = 20000u};

#define DC_SIEGE_OBJECT_TRANSACTION UINT64_C(0x5349450000000001)

static const float DC_SIEGE_COCKED_ANGLE = -0.83f;
static const float DC_SIEGE_SPRING_REST = 0.6f;

typedef struct {
    uint32_t x, y, width, height;
    uint16_t material;
    uint32_t flags;
    float wet;
} siege_rect_t;

static const siege_rect_t siege_floor = {0u, DC_SIEGE_FLOOR_Y, 480u, 40u,
                                          DC_MATERIAL_STONE,
                                          DC_CELL_LOADED_TERRAIN, 0.f};
static const siege_rect_t siege_base = {DC_SIEGE_BASE_X, DC_SIEGE_BASE_Y,
                                        DC_SIEGE_BASE_W, DC_SIEGE_BASE_H,
                                        DC_MATERIAL_STONE,
                                        DC_CELL_LOADED_TERRAIN, 0.f};
static const siege_rect_t siege_posts[2] = {
    {DC_SIEGE_POST_LEFT_X, DC_SIEGE_POST_Y, DC_SIEGE_POST_W, DC_SIEGE_POST_H,
     DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN, 0.f},
    {DC_SIEGE_POST_RIGHT_X, DC_SIEGE_POST_Y, DC_SIEGE_POST_W, DC_SIEGE_POST_H,
     DC_MATERIAL_WOOD, DC_CELL_LOADED_TERRAIN, 0.f}};
static const siege_rect_t siege_bar = {DC_SIEGE_BAR_X, DC_SIEGE_BAR_Y,
                                       DC_SIEGE_BAR_W, DC_SIEGE_BAR_H,
                                       DC_MATERIAL_WOOD,
                                       DC_CELL_LOADED_TERRAIN, 0.f};
/* Rubble pocket at the wall foot: a restrained shot settles the burning
 * ball against the face instead of reflecting it across the ground, so
 * the plume can hold contact for the locked damage window. */
static const siege_rect_t siege_rubble = {DC_SIEGE_WALL_X - 4u,
                                          DC_SIEGE_FLOOR_Y - 25u, 4u, 25u,
                                          DC_MATERIAL_STONE,
                                          DC_CELL_LOADED_TERRAIN, 0.f};

static bool siege_error(char *err, uint32_t cap, const char *message) {
    if (err && cap) snprintf(err, cap, "%s", message);
    return false;
}

static void siege_fill_rect(const siege_rect_t *rect, dc_chunk_t *chunks) {
    for (uint32_t y = rect->y; y < rect->y + rect->height; y++)
        for (uint32_t x = rect->x; x < rect->x + rect->width; x++) {
            uint32_t tile = (y / 64u) * DC_SIEGE_TILES_X + x / 64u;
            dc_cell_t *cell = &chunks[tile].cells[(y % 64u) * 64u + x % 64u];
            cell->material = rect->material;
            cell->flags = rect->flags;
        }
}

static bool siege_stage_region(const dc_chunk_t *chunks, const siege_rect_t *rects,
                               uint32_t rect_count, uint32_t material, bool wet,
                               bool hot, uint64_t *fuel, uint64_t *water,
                               uint32_t *cells, uint64_t *parcel_counter,
                               fr_reactive_chunk_t *packets, char *err, uint32_t cap) {
    const fr_reactive_profile_t *profile = fr_reactive_profile(material);
    if (!profile) return siege_error(err, cap, "Missing siege material profile");
    uint32_t mass = (uint32_t)llround(profile->density * 65536.);
    for (uint32_t index = 0; index < rect_count; index++) {
        const siege_rect_t *rect = &rects[index];
        for (uint32_t y = rect->y; y < rect->y + rect->height; y++)
            for (uint32_t x = rect->x; x < rect->x + rect->width; x++) {
                uint32_t tile = (y / 64u) * DC_SIEGE_TILES_X + x / 64u;
                uint32_t local = (y % 64u) * 64u + x % 64u;
                if (chunks[tile].cells[local].material != (uint16_t)material)
                    continue;
                uint32_t water_fp = wet ? (uint32_t)llround(
                    (double)mass * profile->moisture_capacity * rect->wet) : 0u;
                fr_reactive_state_t state;
                uint64_t parcel = (*parcel_counter)++;
                if (!fr_reactive_state_default(material, mass, water_fp, hot ? 1000.f : 293.15f,
                                               parcel, &state, err, cap))
                    return false;
                if (hot) state.flags |= FR_REACTIVE_FLAMING;
                fr_reactive_chunk_t *packet = &packets[tile];
                fr_reactive_attachment_t *attachment =
                    realloc(packet->attachments,
                            (size_t)(packet->count + 1u) * sizeof(*attachment));
                if (!attachment) return siege_error(err, cap, "Cannot stage siege parcel");
                packet->attachments = attachment;
                fr_reactive_attachment_t *entry =
                    &packet->attachments[packet->count++];
                entry->local_index = local;
                entry->record.owner = fr_reactive_cell_owner(chunks[tile].coord, local);
                entry->record.state = state;
                *fuel += state.fuel_mass_fp + state.char_mass_fp;
                *water += water_fp;
                (*cells)++;
            }
    }
    return true;
}

/* The locked 8x80 wall is five anchored 16-cell wood members. Vertical
 * member boxes sit (len/2+depth/2) above their lower node, so the lower
 * nodes sit at y=242 down to y=178 and the boxes span x154..162, y150..230. */
static fr_structural_gpu_node_t siege_node(uint32_t id, uint32_t x, uint32_t y) {
    return (fr_structural_gpu_node_t){.id = id, .generation = 1,
        .chunk = {(int32_t)(x / DC_CHUNK_SIDE), (int32_t)(y / DC_CHUNK_SIDE)},
        .x_fp = (int32_t)(x % DC_CHUNK_SIDE) * 65536,
        .y_fp = (int32_t)(y % DC_CHUNK_SIDE) * 65536};
}

static fr_structural_gpu_graph_t siege_wall(void) {
    fr_structural_gpu_graph_t graph = {.node_count = 10, .member_count = 5,
                                       .anchor_count = 1};
    for (uint32_t node = 0; node < 5u; node++) {
        graph.nodes[node] = siege_node(node + 1u, 162u, 162u + 16u * node);
        graph.nodes[5u + node] = siege_node(6u + node, 162u, 146u + 16u * node);
    }
    for (uint32_t member = 0; member < 5u; member++)
        graph.members[member] = (fr_structural_gpu_member_t){
            .id = 11u + member, .generation = 1, .a = member + 1u,
            .b = 6u + member, .material = FR_STRUCTURAL_WOOD,
            .depth_fp = DC_SIEGE_WALL_W * 65536};
    graph.anchors[0] = (fr_structural_gpu_anchor_t){
        .id = 1u, .generation = 1, .node = 5u, .kind = FR_JOINT_MOMENT,
        .chunk = {(int32_t)(160u / DC_CHUNK_SIDE), 3},
        .x = 160u % DC_CHUNK_SIDE, .y = 230u % 64u,
        .width = 3, .height = 1, .terrain_material = DC_MATERIAL_STONE,
        .normal_stiffness = 1e8f, .shear_stiffness = 1e8f,
        .rotational_stiffness = 1e8f, .bearing_capacity = 1e7f,
        .pullout_capacity = 1e7f, .shear_capacity = 1e7f,
        .moment_capacity = 1e7f, .friction = .6f,
        .normal_angle = -1.57079632679f};
    return graph;
}

/* The locked launcher frame is fixed constructed terrain with a kinematic
 * pivot mount; the arm, latch and pouch stay real dynamic constraints. */
static const uint32_t DC_SIEGE_MOUNT_ID = 0x70000001u;

static bool dc_siege_scene_build_load_basin(dc_gpu_t *gpu, bool wet_wall,
                                            dc_siege_load_t load, bool douse_pool,
                                            dc_siege_scene_t *out, char *err_buf,
                                            uint32_t err_cap);

bool dc_siege_scene_build(dc_gpu_t *gpu, bool wet_wall, bool hot_coal,
                          dc_siege_scene_t *out, char *err_buf, uint32_t err_cap) {
    (void)hot_coal;
    /* The historical hot-coal flag maps to the coal load; the Greek module
     * stages oil and greek loads through dc_siege_scene_build_load. */
    return dc_siege_scene_build_load(gpu, wet_wall, DC_SIEGE_LOAD_COAL,
                                     out, err_buf, err_cap);
}

bool dc_siege_scene_ignite_load(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene || !scene->coal_body)
        return siege_error(err_buf, err_cap, "Missing siege load to ignite");
    for (uint32_t py = 0; py < 2u * DC_SIEGE_COAL_RADIUS + 1u; py++)
        for (uint32_t px = 0; px < 2u * DC_SIEGE_COAL_RADIUS + 1u; px++) {
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_body(gpu, scene->coal_body, px, py,
                                           &parcel, &found, err_buf, err_cap))
                return false;
            if (!found || parcel.state.coating_oil_fuel_fp == 0u) continue;
            if (!fr_reactive_state_valid(&parcel.state, err_buf, err_cap))
                return siege_error(err_buf, err_cap, "Load parcel invalid pre-ignition");
            /* The fuse both flags the film and strikes it to its ignition
             * point: a cold film cannot sustain the reaction. The soak
             * water flashes off instantly (the boil is magical — no
             * per-tick steam spike for the transport to choke on); the
             * pool rewets the film gradually and the burning film eats
             * the inflow. */
            double hot = fr_reactive_state_energy_at_temperature(
                &parcel.state, 530.f);
            double current = fr_reactive_state_energy(&parcel.state);
            if (hot > current) {
                char add_err[128] = {0};
                if (!fr_reactive_state_add_heat(&parcel.state, hot - current,
                                                add_err, sizeof(add_err))) {
                    fprintf(stderr, "ignite %u,%u add failed: %s\n", px, py,
                            add_err);
                    snprintf(err_buf, err_cap, "%s", add_err);
                    return false;
                }
            }
            /* The boil-off is magical: the soak water flashes off instantly
             * rather than choking the transport with a steam spike; the
             * pool rewets gradually and the burning film eats the inflow. */
            parcel.state.bound_water_fp = 0u;
            parcel.state.evaporation_fraction = 0.f;
            if (getenv("DC_GREEK_TRACE")) {
                char why[128] = {0};
                fr_reactive_state_valid(&parcel.state, why, sizeof(why));
                fprintf(stderr, "ignite %u,%u dry valid %d (%s)\n", px, py,
                        fr_reactive_state_valid(&parcel.state, NULL, 0u) ? 1
                                                                          : 0,
                        why);
            }
            parcel.state.flags |= FR_REACTIVE_COATING_FLAMING;
            parcel.state.coating_threshold = .5f;
            parcel.state.coating_integral = .6f;
            if (!fr_gpu_reactive_seed_body(gpu, scene->coal_body, px, py,
                                           &parcel.state, err_buf, err_cap))
                return false;
        }
    return true;
}

/* Matched coating fuel: the oil and greek loads carry the same total film
 * mass (oil 65536 fp versus oil 32768 + jelly 32768) so the depletion-pace
 * comparison isolates the chemistry. The core is inert stone; the coating
 * is the only fuel and arrives pre-armed like the coal. */
static void siege_arm_coating(fr_reactive_state_t *state, dc_siege_load_t load) {
    if (load == DC_SIEGE_LOAD_OIL) {
        state->coating_oil_fuel_fp = 65536u;
    } else if (load == DC_SIEGE_LOAD_GREEK) {
        state->coating_oil_fuel_fp = 32768u;
        state->coating_jelly_fuel_fp = 32768u;
        state->coating_oxidizer_fp = 32768u;
    } else {
        return;
    }
    state->coating_threshold = .01f;
    state->coating_integral = .02f;
    /* The fuse stays cold in the pouch; dc_siege_scene_ignite_load lights
     * the film at the scenario's chosen moment (prompt launch, delayed
     * hold or never). A cold swing keeps the trajectory deterministic. */
}

bool dc_siege_scene_build_load(dc_gpu_t *gpu, bool wet_wall, dc_siege_load_t load,
                               dc_siege_scene_t *out, char *err_buf,
                               uint32_t err_cap) {
    return dc_siege_scene_build_load_basin(gpu, wet_wall, load, false, out,
                                           err_buf, err_cap);
}

bool dc_siege_scene_build_douse_pool(dc_gpu_t *gpu, dc_siege_load_t load,
                                     dc_siege_scene_t *out, char *err_buf,
                                     uint32_t err_cap) {
    return dc_siege_scene_build_load_basin(gpu, false, load, true, out,
                                           err_buf, err_cap);
}

static bool dc_siege_scene_build_load_basin(dc_gpu_t *gpu, bool wet_wall,
                                            dc_siege_load_t load, bool douse_pool,
                                            dc_siege_scene_t *out, char *err_buf,
                                            uint32_t err_cap) {
    if (!gpu) return siege_error(err_buf, err_cap, "Missing siege GPU context");
    bool hot_coal = load == DC_SIEGE_LOAD_COAL;
    (void)hot_coal;
    dc_chunk_t *chunks = calloc(DC_SIEGE_TILE_COUNT, sizeof(*chunks));
    dc_chunk_colors_t *colors = calloc(DC_SIEGE_TILE_COUNT, sizeof(*colors));
    fr_reactive_chunk_t *packets = calloc(DC_SIEGE_TILE_COUNT, sizeof(*packets));
    uint32_t *objects = calloc(DC_GPU_CHUNK_SLOTS * 4096u, sizeof(*objects));
    if (!chunks || !colors || !packets || !objects) {
        free(chunks);free(colors);free(packets);free(objects);
        return siege_error(err_buf, err_cap, "Cannot allocate siege staging");
    }
    for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
        chunks[tile].coord = (dc_chunk_coord_t){tile % DC_SIEGE_TILES_X,
                                                tile / DC_SIEGE_TILES_X};
    siege_fill_rect(&siege_floor, chunks);
    siege_fill_rect(&siege_base, chunks);
    siege_fill_rect(&siege_posts[0], chunks);
    siege_fill_rect(&siege_posts[1], chunks);
    siege_fill_rect(&siege_bar, chunks);
    siege_fill_rect(&siege_rubble, chunks);
    siege_rect_t arm = {DC_SIEGE_ARM_X, DC_SIEGE_ARM_Y, DC_SIEGE_ARM_W,
                        DC_SIEGE_ARM_H, DC_MATERIAL_WOOD, 0u, 0.f};
    siege_fill_rect(&arm, chunks);
    for (uint32_t y = DC_SIEGE_ARM_Y; y < DC_SIEGE_ARM_Y + DC_SIEGE_ARM_H; y++)
        for (uint32_t x = DC_SIEGE_ARM_X; x < DC_SIEGE_ARM_X + DC_SIEGE_ARM_W; x++) {
            uint32_t tile = (y / 64u) * DC_SIEGE_TILES_X + x / 64u;
            objects[tile * 4096u + (y % 64u) * 64u + x % 64u] =
                DC_SIEGE_OBJECT_ARM;
        }
    /* The cocked ball rests west of the pivot, clear of the wall face. The
     * douse fixture stages the load directly inside a standing pool so the
     * submersion scenario is exact and swing-free. */
    int32_t ball_x = DC_SIEGE_PIVOT_X +
                     (int32_t)lroundf(40.f * sinf(DC_SIEGE_COCKED_ANGLE));
    int32_t ball_y = DC_SIEGE_PIVOT_Y -
                     (int32_t)lroundf(40.f * cosf(-DC_SIEGE_COCKED_ANGLE));
    if (douse_pool) {ball_x = 32; ball_y = 215;}
    if (douse_pool)
        for (uint32_t y = 209u; y <= 220u; y++)
            for (uint32_t x = 24u; x <= 40u; x++) {
                uint32_t tile = (y / 64u) * DC_SIEGE_TILES_X + x / 64u;
                uint32_t local = (y % 64u) * 64u + x % 64u;
                chunks[tile].cells[local].material = DC_MATERIAL_WATER;
                chunks[tile].cells[local].fluid_mass = DC_FLUID_FULL;
            }
    uint32_t coal_cells = 0;
    for (int32_t y = ball_y - DC_SIEGE_COAL_RADIUS;
         y <= ball_y + DC_SIEGE_COAL_RADIUS; y++)
        for (int32_t x = ball_x - DC_SIEGE_COAL_RADIUS;
             x <= ball_x + DC_SIEGE_COAL_RADIUS; x++) {
            int32_t dx = x - ball_x, dy = y - ball_y;
            if (dx * dx + dy * dy > DC_SIEGE_COAL_RADIUS * DC_SIEGE_COAL_RADIUS)
                continue;
            uint32_t tile = ((uint32_t)y / 64u) * DC_SIEGE_TILES_X + (uint32_t)x / 64u;
            uint32_t local = ((uint32_t)y % 64u) * 64u + (uint32_t)x % 64u;
            uint16_t core = douse_pool
                                ? (uint16_t)DC_MATERIAL_WOOD
                                : load == DC_SIEGE_LOAD_COAL
                                      ? (uint16_t)DC_MATERIAL_COAL
                                      : (uint16_t)DC_MATERIAL_STONE;
            chunks[tile].cells[local].material = core;
            chunks[tile].cells[local].flags = 0u;
            /* The rigid load displaces the pool water in its own cells. */
            chunks[tile].cells[local].fluid_mass = 0u;
            objects[tile * 4096u + local] = DC_SIEGE_OBJECT_COAL;
            coal_cells++;
        }
    for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
        for (uint32_t i = 0; i < 4096u; i++) {
            uint32_t x = chunks[tile].coord.x * 64u + i % 64u;
            uint32_t y = chunks[tile].coord.y * 64u + i / 64u;
            float grain = (float)(((uint64_t)x * 3u + (uint64_t)y * 11u) & 15u) / 255.f;
            float *color = colors[tile].terrain[i];
            uint32_t material = chunks[tile].cells[i].material;
            if (material == DC_MATERIAL_WOOD) {
                color[0] = .35f;color[1] = .22f;color[2] = .10f;
            } else if (material == DC_MATERIAL_STONE) {
                color[0] = .23f;color[1] = .25f;color[2] = .28f;
            } else if (material == DC_MATERIAL_COAL) {
                color[0] = .12f;color[1] = .11f;color[2] = .11f;
            } else if (material == DC_MATERIAL_WATER) {
                color[0] = .10f;color[1] = .18f;color[2] = .30f;
            } else {
                color[0] = .07f;color[1] = .07f;color[2] = .09f;
            }
            color[0] += grain;color[1] += grain;color[2] += grain;color[3] = 1.f;
        }
    dc_siege_scene_t scene = {0};
    uint64_t ignored = 0;
    uint32_t ignored_cells = 0;
    uint64_t parcel_counter = 6ull << 58;
    /* Wall fuel lives in constructed member parcels, not staged cells. */
    scene.wall_cells = DC_SIEGE_WALL_W * DC_SIEGE_WALL_H;
    scene.wall_fuel_fp = (uint64_t)scene.wall_cells * 39322u;
    scene.wall_water_fp = wet_wall
                              ? (uint64_t)scene.wall_cells *
                                    ((uint32_t)llround(
                                        fr_reactive_profile(DC_MATERIAL_WOOD)
                                            ->moisture_capacity * 65536.f)) *
                                    95u / 100u
                              : 0u;
    bool okay = true;
    /* The submerged douse variant uses a wood core so the pool soaks into
     * the load; the dry pace comparison keeps inert stone cores. */
    uint16_t core_material = load == DC_SIEGE_LOAD_COAL
                                 ? (uint16_t)DC_MATERIAL_COAL
                                 : douse_pool ? (uint16_t)DC_MATERIAL_WOOD
                                              : (uint16_t)DC_MATERIAL_STONE;
    const siege_rect_t coal_rects[1] = {{(uint32_t)(ball_x - DC_SIEGE_COAL_RADIUS),
                                         (uint32_t)(ball_y - DC_SIEGE_COAL_RADIUS),
                                         2u * DC_SIEGE_COAL_RADIUS + 1u,
                                         2u * DC_SIEGE_COAL_RADIUS + 1u,
                                         core_material, 0u, 0.f}};
    uint64_t coal_fuel = 0;
    uint32_t staged_cells = 0;
    okay = okay && siege_stage_region(chunks, coal_rects, 1u, core_material,
                                      false, load == DC_SIEGE_LOAD_COAL,
                                      &coal_fuel, &ignored,
                                      &staged_cells, &parcel_counter, packets,
                                      err_buf, err_cap);
    if (okay && (load == DC_SIEGE_LOAD_OIL || load == DC_SIEGE_LOAD_GREEK))
        for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
            for (uint32_t i = 0; i < packets[tile].count; i++) {
                fr_reactive_state_t *state =
                    &packets[tile].attachments[i].record.state;
                if (state->material == core_material)
                    siege_arm_coating(state, load);
            }
    if (okay && hot_coal)
        /* Pre-armed locked ignition: the burning ball is viable from its
         * first admitted tick (vent-case pattern), not a fresh hazard roll. */
        for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
            for (uint32_t i = 0; i < packets[tile].count; i++) {
                fr_reactive_state_t *state =
                    &packets[tile].attachments[i].record.state;
                if (state->material == DC_MATERIAL_COAL &&
                    (state->flags & FR_REACTIVE_FLAMING) != 0u) {
                    state->ignition_threshold = .5f;
                    state->ignition_integral = .6f;
                }
            }
    uint64_t arm_fuel = 0;
    const siege_rect_t arm_rects[1] = {arm};
    okay = okay && siege_stage_region(chunks, arm_rects, 1u, DC_MATERIAL_WOOD,
                                      false, false, &arm_fuel, &ignored,
                                      &ignored_cells, &parcel_counter, packets,
                                      err_buf, err_cap);
    scene.coal_cells = coal_cells;
    (void)coal_fuel;
    uint32_t total = 0;
    for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
        total += packets[tile].count;
    if (okay) okay = fr_gpu_reactive_reserve_import(gpu, total, err_buf, err_cap);
    for (uint32_t tile = 0; okay && tile < DC_SIEGE_TILE_COUNT; tile++) {
        packets[tile].next_id = parcel_counter;
        if (!packets[tile].count) continue;
        okay = dc_gpu_upload_chunk(gpu, tile, &chunks[tile], err_buf, err_cap) &&
               dc_gpu_set_page(gpu, tile % DC_SIEGE_TILES_X,
                               tile / DC_SIEGE_TILES_X, tile, err_buf, err_cap) &&
               fr_gpu_reactive_import_chunk(gpu, tile, &chunks[tile],
                                            &packets[tile], err_buf, err_cap);
    }
    for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
        if (!packets[tile].count || !okay)
            fr_reactive_chunk_release(&packets[tile]);
    for (uint32_t tile = 0; okay && tile < DC_SIEGE_TILE_COUNT; tile++) {
        if (packets[tile].count) continue;
        okay = dc_gpu_upload_chunk(gpu, tile, &chunks[tile], err_buf, err_cap) &&
               dc_gpu_set_page(gpu, tile % DC_SIEGE_TILES_X,
                               tile / DC_SIEGE_TILES_X, tile, err_buf, err_cap);
    }
    for (uint32_t tile = 0; tile < DC_SIEGE_TILE_COUNT; tile++)
        fr_reactive_chunk_release(&packets[tile]);
    free(packets);
    packets = NULL;
    free(chunks);
    chunks = NULL;
    if (okay) {
        fr_structural_gpu_graph_t wall = siege_wall();
        okay = fr_thermal_gpu_enable(gpu, err_buf, err_cap) &&
               dc_gpu_set_rigid_solver(gpu, true) &&
               fr_gpu_place_structure(gpu, &wall, err_buf, err_cap) &&
               fr_gpu_reactive_admit_constructed_members(gpu, err_buf, err_cap);
    }
    if (okay && wet_wall) {
        const fr_reactive_profile_t *profile = fr_reactive_profile(DC_MATERIAL_WOOD);
        uint32_t mass = (uint32_t)llround(profile->density * 65536.);
        uint32_t water = (uint32_t)llround((double)mass *
                                           profile->moisture_capacity * .95);
        for (uint32_t member = 0; okay && member < 5u; member++)
            for (uint32_t y = 0; y < DC_SIEGE_WALL_W; y++)
                for (uint32_t x = 0; x < 16u; x++) {
                    fr_reactive_state_t state;
                    okay = fr_reactive_state_default(
                               DC_MATERIAL_WOOD, mass, water, 293.15f,
                               parcel_counter++, &state, err_buf, err_cap) &&
                           fr_gpu_reactive_seed_body(
                               gpu, FR_STRUCTURAL_BODY_ID_BASE + 11u + member,
                               x, y, &state, err_buf, err_cap);
                }
    }
    if (okay) {
        dc_gpu_world_body_t mount = {.chunk = {1, 2},
            .body = {.id = DC_SIEGE_MOUNT_ID,
                     .x_fp = (int32_t)(DC_SIEGE_PIVOT_X % 64u) * 65536 - 2 * 65536,
                     .y_fp = (int32_t)(DC_SIEGE_PIVOT_Y % 64u) * 65536 - 65536,
                     .width = 4, .height = 2, .active = 1}};
        dc_gpu_body_shape_t mount_shape = {.count = 4,
            .material = DC_GPU_BODY_STONE,
            .vertices = {{0, 0}, {4 * 65536, 0}, {4 * 65536, 2 * 65536},
                         {0, 2 * 65536}}};
         okay = fr_thermal_gpu_enable(gpu, err_buf, err_cap) &&
                dc_gpu_set_rigid_solver(gpu, true) &&
                dc_gpu_spawn_convex_body(gpu, mount, &mount_shape, err_buf,
                                         err_cap) &&
                dc_gpu_set_body_motion(gpu, DC_SIEGE_MOUNT_ID, 0.f, 0.f,
                                       DC_GPU_BODY_KINEMATIC, err_buf, err_cap) &&
                fr_gpu_admit_rigid_objects(gpu, DC_SIEGE_OBJECT_TRANSACTION,
                                           objects,
                                           DC_GPU_CHUNK_SLOTS * 4096u,
                                           err_buf, err_cap);
    }
    free(objects);
    objects = NULL;
    for (uint32_t tick = 0; okay && tick < 5u; tick++)
        okay = dc_gpu_rigid_step(gpu, err_buf, err_cap);
    if (okay) {
        for (uint32_t probe = 0x80000001u; probe <= 0x80000008u; probe++) {
            dc_gpu_world_body_t body;
            if (!dc_gpu_read_world_body(gpu, probe, &body, err_buf, err_cap))
                continue;
            float x = (float)body.chunk.x * 64.f + body.body.x_fp / 65536.f;
            float y = (float)body.chunk.y * 64.f + body.body.y_fp / 65536.f;
            if (getenv("DC_SIEGE_TRACE"))
                printf("siege probe id %x pos %.1f %.1f size %u %u\n", probe, x, y,
                       body.body.width, body.body.height);
            if (body.body.width == DC_SIEGE_ARM_W &&
                body.body.height == DC_SIEGE_ARM_H &&
                scene.arm_body == 0u)
                scene.arm_body = probe;
            else if (body.body.width <= 2u * DC_SIEGE_COAL_RADIUS + 1u &&
                     body.body.height <= 2u * DC_SIEGE_COAL_RADIUS + 1u &&
                     scene.coal_body == 0u)
                scene.coal_body = probe;
        }
        okay = scene.coal_body != 0u && scene.arm_body != 0u;
        if (!okay) siege_error(err_buf, err_cap, "Siege bodies were not admitted");
    }
    if (okay) {
        fr_revolute_joint_t pivot = {.id = 1u,
            .body_a = DC_SIEGE_MOUNT_ID, .body_b = scene.arm_body,
            .anchor_a = {0.f, 0.f}, .anchor_b = {0.f, 19.f},
            .flags = FR_REVOLUTE_SPRING | FR_REVOLUTE_LIMITS,
            .lower_angle = -1.2f, .upper_angle = 1.1f,
            .spring_stiffness = (float)DC_SIEGE_SPRING_STIFFNESS,
            .spring_rest_angle = DC_SIEGE_SPRING_REST};
        float held = DC_SIEGE_COCKED_ANGLE;
        fr_revolute_joint_t latch = {.id = 2u,
            .body_a = DC_SIEGE_MOUNT_ID, .body_b = scene.arm_body,
            .anchor_a = {1.f, 0.f}, .anchor_b = {1.f, 19.f},
            .flags = FR_REVOLUTE_LIMITS | FR_REVOLUTE_SPRING,
            .lower_angle = held, .upper_angle = held,
            .spring_stiffness = 20000.f, .spring_rest_angle = 0.f};
        fr_revolute_joint_t pouch = {.id = 3u,
            .body_a = scene.arm_body, .body_b = scene.coal_body,
            .anchor_a = {0.f, -22.f}, .anchor_b = {0.f, 0.f}};
        okay = fr_gpu_revolute_add(gpu, &pivot, err_buf, err_cap) &&
               fr_gpu_revolute_add(gpu, &latch, err_buf, err_cap) &&
               fr_gpu_revolute_add(gpu, &pouch, err_buf, err_cap) &&
               dc_gpu_set_body_motion(gpu, scene.arm_body, DC_SIEGE_COCKED_ANGLE,
                                      0.f, 0u, err_buf, err_cap);
        scene.pivot_joint = pivot.id;
        scene.latch_joint = latch.id;
        scene.pouch_joint = pouch.id;
    }
    if (okay && !getenv("DC_SIEGE_NO_GAS")) {
        okay = dc_smoke_profile_defaults() &&
               fr_gas_gpu_enable(gpu, &(fr_gas_model_t){
                                          .gas_constant = {.25984, .29680, .18892, .46152, 0},
                                          .heat_capacity_cv = {.65816, .74320, .65508, 1.40248, .71},
                                          .kinetic_scale = 1. / 512.},
                                 .0012f, 293.15f, err_buf, err_cap) &&
               fr_gas_gpu_set_pressure_policy(gpu, FR_GAS_PRESSURE_PRACTICAL,
                                              err_buf, err_cap) &&
               fr_gas_gpu_set_gravity(gpu, 900.f, err_buf, err_cap) &&
               fr_combustion_gpu_enable(gpu, err_buf, err_cap);
    }
#ifdef DC_SIEGE_PRESENTATION
    if (okay) {
        okay = fr_renderer_enable(gpu, err_buf, err_cap);
        for (uint32_t tile = 0; okay && tile < DC_SIEGE_TILE_COUNT; tile++)
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

bool dc_siege_scene_release(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                            char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene || scene->latch_joint == 0u)
        return siege_error(err_buf, err_cap, "Invalid siege latch release");
    bool active = false;
    if (!fr_gpu_revolute_active(gpu, scene->latch_joint, &active, err_buf, err_cap))
        return false;
    if (!active) return true;
    if (!fr_gpu_revolute_remove(gpu, scene->latch_joint, err_buf, err_cap))
        return false;
    return true;
}

bool dc_siege_scene_release_projectile(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                       char *err_buf, uint32_t err_cap) {
    if (!gpu || !scene || scene->pouch_joint == 0u)
        return siege_error(err_buf, err_cap, "Invalid siege pouch release");
    bool active = false;
    if (!fr_gpu_revolute_active(gpu, scene->pouch_joint, &active, err_buf, err_cap))
        return false;
    if (!active) return true;
    if (!fr_gpu_revolute_remove(gpu, scene->pouch_joint, err_buf, err_cap))
        return false;
    return true;
}
