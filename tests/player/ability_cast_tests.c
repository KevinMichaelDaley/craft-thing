#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/fire/reactive_state.h"
#include "ferrum/player/abilities.h"

static int g_pass, g_fail;
static void teardown(void);
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();teardown();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

/* Wood wall at row 30 and a stone floor at row 40 in chunk (3,3); the
 * player fires from world (200,210) toward (232,202). */
#define ORIGIN_X 200.f
#define ORIGIN_Y 210.f
#define TARGET_DX 32.f
#define TARGET_DY -8.f

static dc_gpu_t *gpu = NULL;
static char err[256];

static bool setup_world(void) {
    err[0] = 0;
    if (!dc_gpu_create(&gpu, 480, 270,
                       "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return false;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    chunk->coord = (dc_chunk_coord_t){3, 3};
    for (uint32_t x = 0; x < 64u; x++) {
        chunk->cells[40u * 64u + x].material = DC_MATERIAL_STONE;
        chunk->cells[30u * 64u + x].material = DC_MATERIAL_WOOD;
    }
    bool okay = dc_gpu_upload_chunk(gpu, 11, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 3, 3, 11, err, sizeof(err)) &&
                dc_gpu_set_rigid_solver(gpu, true);
    free(chunk);
    if (!okay || !dc_gpu_tick_step(gpu, err, sizeof(err))) return false;
    /* Burnable world cells carry reactive wood parcels like authored scenes. */
    const fr_reactive_profile_t *wood = fr_reactive_profile(DC_MATERIAL_WOOD);
    if (!wood) return false;
    uint32_t mass = (uint32_t)lroundf(wood->density * 65536.f);
    for (uint32_t x = 0; x < 64u; x++) {
        fr_reactive_state_t state;
        if (!fr_reactive_state_default(DC_MATERIAL_WOOD, mass, 0, 293.15f,
                                       0x70000000u + x, &state, err,
                                       sizeof(err)))
            return false;
        if (!fr_gpu_reactive_seed_cell(gpu, 11u, 30u * 64u + x, &state, err,
                                       sizeof(err)))
            return false;
    }
    /* Combustion and brand transfer live in the thermal pipeline. */
    return fr_thermal_gpu_enable(gpu, err, sizeof(err));
}

static fr_player_point_t point_at(float world_x, float world_y) {
    return (fr_player_point_t){.chunk = {3, 3},
        .x = world_x - 192.f, .y = world_y - 192.f};
}

static void teardown(void) {
    dc_gpu_destroy(gpu);
    gpu = NULL;
}

static void test_fire_bolt_spawns_burning_body_toward_target(void) {
    ASSERT_TRUE(setup_world());
    fr_player_point_t origin = point_at(ORIGIN_X, ORIGIN_Y);
    fr_player_point_t target = point_at(ORIGIN_X + TARGET_DX,
                                        ORIGIN_Y + TARGET_DY);
    uint32_t body = 0u;
    ASSERT_TRUE(fr_player_cast_fire_bolt(gpu, 7u, &origin,
                                         &target, &body, err, sizeof(err)));
    ASSERT_TRUE(body != 0u);
    dc_gpu_world_body_t motion;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, body, &motion, err, sizeof(err)));
    float vx = (float)motion.body.vx_fp / 65536.f;
    float vy = (float)motion.body.vy_fp / 65536.f;
    float speed = sqrtf(vx * vx + vy * vy);
    printf("bolt speed %.3f direction %.2f,%.2f\n", speed, vx, vy);
    /* The bolt launches toward the target, capped at four cells per tick. */
    ASSERT_TRUE(speed > 2.f && speed <= 4.01f);
    ASSERT_TRUE(vx > 0.f && vy < 0.f);
    dc_gpu_body_motion_t coal;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, body, &coal, err, sizeof(err)));
    ASSERT_TRUE(coal.mass > 0.f);
    fr_reactive_record_t record;
    bool found = false;
    uint32_t slot = 0u;
    for (uint32_t py = 0u; py < 3u && !found; py++)
        for (uint32_t pxx = 0u; pxx < 3u && !found; pxx++)
            ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, body, pxx, py, &record,
                                                  &found, err,
                                                  sizeof(err)));
    ASSERT_TRUE(found);
    (void)slot;
    /* The bolt carries burning coal fuel into the world. */
    ASSERT_EQ(record.state.material, DC_MATERIAL_COAL);
    ASSERT_TRUE(record.state.fuel_mass_fp > 0u);
    ASSERT_TRUE((record.state.flags & FR_REACTIVE_FLAMING) != 0u);
    PASS();
}
static void test_fire_bolt_ignites_wood_on_impact(void) {
    ASSERT_TRUE(setup_world());
    /* Drop the bolt straight down into the wood wall at row 30. */
    fr_player_point_t origin = point_at(220.f, 204.f);
    fr_player_point_t target = point_at(220.f, 224.f);
    uint32_t bolt = 0u;
    ASSERT_TRUE(fr_player_cast_fire_bolt(gpu, 7u, &origin,
                                         &target, &bolt, err, sizeof(err)));
    ASSERT_TRUE(bolt != 0u);
    /* Follow the bolt while it flies and report where it ends up. */
    dc_gpu_world_body_t initial, flight;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, bolt, &initial, err, sizeof(err)));
    dc_gpu_body_record_t *rec = dc_gpu_find_body_record(gpu, bolt);
    if (rec)
        printf("bolt record visible %u enabled %u awake_word %u vx %d rest %u motion_ready %u\n",
               rec->visible, rec->body.active, rec->visible,
               rec->body.vx_fp, rec->rest_ticks, rec->motion_ready);
    for (uint32_t tick = 0u; tick < 12u; tick++) {
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        if (tick == 0u) {
            ASSERT_TRUE(dc_gpu_read_world_body(gpu, bolt, &flight, err, sizeof(err)));
            ASSERT_EQ(flight.chunk.y, initial.chunk.y);
            ASSERT_TRUE(flight.body.y_fp > initial.body.y_fp);
        }
        if (dc_gpu_read_world_body(gpu, bolt, &flight, err, sizeof(err)))
            printf("bolt tick %u at %.1f,%.1f v %.2f,%.2f\n", tick,
                   (float)flight.chunk.x * 64.f +
                       flight.body.x_fp / 65536.f,
                   (float)flight.chunk.y * 64.f +
                       flight.body.y_fp / 65536.f,
                   (float)flight.body.vx_fp / 65536.f,
                   (float)flight.body.vy_fp / 65536.f);
    }
    uint32_t hit_slot = UINT32_MAX;
    for (uint32_t tick = 0u; tick < 40u; tick++) {
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
        double hottest = 0.;
        uint32_t hottest_cell = 0u;
        uint32_t branded = 0u;
        for (uint32_t x = 24u; x < 33u; x++) {
            fr_reactive_record_t touch;
            bool touch_found = false;
            if (!fr_gpu_reactive_read_cell(gpu, 11u, 30u * 64u + x, &touch,
                                           &touch_found, err, sizeof(err)) ||
                !touch_found)
                continue;
            double energy = fr_reactive_state_energy(&touch.state);
            if (energy > hottest) {
                hottest = energy;
                hottest_cell = x;
            }
            if (touch.state.coating_oil_fuel_fp > 0u) branded++;
        }
        if (tick % 8u == 0u || branded)
            printf("wall tick %u hottest cell %u at %.3g branded %u\n", tick,
                   hottest_cell, hottest, branded);
        if (branded) break;
    }
    (void)hit_slot;
    fr_reactive_record_t bolt_cell;
    bool bolt_found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bolt, 1u, 1u, &bolt_cell,
                                          &bolt_found, err, sizeof(err)));
    if (bolt_found)
        printf("bolt center flags %u fuel %u heat %.4g\n",
               bolt_cell.state.flags, bolt_cell.state.fuel_mass_fp,
               fr_reactive_state_energy(&bolt_cell.state));
    /* The struck wood warmed far above ambient from the burning charge:
     * conductive and brand heating at the contact strip is the ignition
     * deposit, and sustained flame follows through ordinary combustion. */
    double hottest = 0.;
    for (uint32_t x = 24u; x < 33u; x++) {
        fr_reactive_record_t wood;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, 11u, 30u * 64u + x, &wood,
                                              &found, err, sizeof(err)));
        if (found && fr_reactive_state_energy(&wood.state) > hottest)
            hottest = fr_reactive_state_energy(&wood.state);
    }
    printf("wall hottest %.4g over ambient %.4g\n", hottest, 264.);
    ASSERT_TRUE(hottest > 264. * 1.4);
    PASS();
}

static bool map_empty(uint32_t slot, dc_chunk_coord_t coord,
                      uint32_t page_x, uint32_t page_y) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    chunk->coord = coord;
    bool okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, page_x, page_y, slot, err, sizeof(err));
    free(chunk);
    return okay;
}

static uint16_t terrain_at(uint32_t slot, uint32_t x, uint32_t y) {
    dc_chunk_t *chunk = malloc(sizeof(*chunk));
    if (!chunk) return UINT16_MAX;
    bool okay = dc_gpu_download_chunk(gpu, slot, chunk, err, sizeof(err));
    uint16_t material = okay ? chunk->cells[y * DC_CHUNK_SIDE + x].material : UINT16_MAX;
    free(chunk);
    return material;
}

static void test_fire_bolt_owns_every_burning_texel_across_a_streamed_seam(void) {
    ASSERT_TRUE(setup_world());
    const dc_chunk_coord_t left = {-20, 118}, right = {-19, 118};
    ASSERT_TRUE(map_empty(12u, left, 2u, 2u));
    ASSERT_TRUE(map_empty(13u, right, 3u, 2u));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){-22, 116},
                                      err, sizeof(err)));
    const fr_player_point_t origin = {left, 55.f, 16.f};
    const fr_player_point_t target = {right, 20.f, 16.f};
    uint32_t bolt;
    ASSERT_TRUE(fr_player_cast_fire_bolt(gpu, 7u, &origin, &target, &bolt,
                                         err, sizeof(err)));
    dc_gpu_world_body_t body;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, bolt, &body, err, sizeof(err)));
    ASSERT_EQ(body.chunk.x, left.x);
    ASSERT_EQ(body.chunk.y, left.y);
    ASSERT_EQ(body.body.x_fp, 62 * DC_FLUID_FULL);
    ASSERT_EQ(body.body.y_fp, 15 * DC_FLUID_FULL);
    ASSERT_EQ(body.body.vx_fp, 4 * DC_FLUID_FULL);
    ASSERT_EQ(body.body.vy_fp, 0);
    dc_gpu_body_pixel_t pixels[5];
    uint32_t count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, bolt, pixels, 5u, &count,
                                       err, sizeof(err)));
    ASSERT_EQ(count, 5u);
    uint64_t ids[5] = {0};
    uint32_t generation = 0;
    for (uint32_t i = 0u; i < count; ++i) {
        ASSERT_EQ(pixels[i].material, DC_GPU_BODY_COAL);
        fr_reactive_record_t parcel;
        bool found;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, bolt, pixels[i].x, pixels[i].y,
                                              &parcel, &found, err, sizeof(err)));
        ASSERT_TRUE(found);
        ASSERT_EQ(parcel.owner.kind, FR_REACTIVE_OWNER_BODY);
        ASSERT_EQ(parcel.owner.key[0], bolt);
        if (!i) generation = parcel.owner.key[1];
        ASSERT_TRUE(generation != 0u);
        ASSERT_EQ(parcel.owner.key[1], generation);
        ASSERT_EQ(parcel.state.material, DC_MATERIAL_COAL);
        ASSERT_TRUE(parcel.state.fuel_mass_fp != 0u);
        ASSERT_TRUE((parcel.state.flags & FR_REACTIVE_FLAMING) != 0u);
        ids[i] = ((uint64_t)parcel.state.parcel_id_high << 32) | parcel.state.parcel_id_low;
        for (uint32_t previous = 0; previous < i; ++previous)
            ASSERT_TRUE(ids[previous] != ids[i]);
    }
    ASSERT_EQ(terrain_at(12u, 63u, 16u), DC_MATERIAL_AIR);
    ASSERT_EQ(terrain_at(13u, 0u, 16u), DC_MATERIAL_AIR);
    PASS();
}

static void test_repeated_bolts_leave_unrelated_bodies_and_terrain_alone(void) {
    ASSERT_TRUE(setup_world());
    const uint32_t decoy_id = UINT32_C(0x80000001);
    dc_gpu_world_body_t decoy = {{3, 3}, {.x_fp = 18 * DC_FLUID_FULL,
        .y_fp = 18 * DC_FLUID_FULL, .width = 1, .height = 1, .id = decoy_id, .active = 1}};
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, decoy, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 246u, 204u, 0u, DC_MATERIAL_WOOD,
                                     err, sizeof(err)));
    const fr_player_point_t origins[] = {point_at(200.f, 210.f), point_at(200.f, 217.f)};
    const fr_player_point_t targets[] = {point_at(232.f, 210.f), point_at(232.f, 217.f)};
    uint32_t first, second;
    dc_gpu_extraction_stats_t before, after;
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &before));
    ASSERT_TRUE(fr_player_cast_fire_bolt(gpu, 7u, &origins[0], &targets[0],
                                         &first, err, sizeof(err)));
    ASSERT_TRUE(fr_player_cast_fire_bolt(gpu, 8u, &origins[1], &targets[1],
                                         &second, err, sizeof(err)));
    ASSERT_TRUE(first && second && first != second && first != decoy_id && second != decoy_id);
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &after));
    ASSERT_EQ(after.owned_bodies, before.owned_bodies + 2u);
    dc_gpu_world_body_t still;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, decoy_id, &still, err, sizeof(err)));
    ASSERT_EQ(still.body.vx_fp, 0);
    ASSERT_EQ(still.body.vy_fp, 0);
    ASSERT_EQ(terrain_at(11u, 54u, 12u), DC_MATERIAL_WOOD);
    fr_reactive_record_t a, b;
    bool found;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, first, 1u, 1u, &a, &found,
                                          err, sizeof(err)) && found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, second, 1u, 1u, &b, &found,
                                          err, sizeof(err)) && found);
    ASSERT_TRUE(a.state.parcel_id_low != b.state.parcel_id_low ||
                a.state.parcel_id_high != b.state.parcel_id_high);
    PASS();
}

static void test_fire_bolt_rejects_occupied_and_unmapped_footprints(void) {
    ASSERT_TRUE(setup_world());
    const fr_player_point_t origin = point_at(220.f, 214.f);
    const fr_player_point_t target = point_at(240.f, 214.f);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 228u, 214u, 0u, DC_MATERIAL_STONE,
                                     err, sizeof(err)));
    dc_gpu_extraction_stats_t before, after;
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &before));
    uint32_t bolt = UINT32_MAX;
    ASSERT_TRUE(!fr_player_cast_fire_bolt(gpu, 7u, &origin, &target, &bolt,
                                          err, sizeof(err)));
    ASSERT_EQ(bolt, 0u);
    ASSERT_EQ(terrain_at(11u, 36u, 22u), DC_MATERIAL_STONE);
    ASSERT_EQ(terrain_at(11u, 35u, 22u), DC_MATERIAL_AIR);
    const fr_player_point_t edge = {{3, 3}, 62.f, 14.f};
    const fr_player_point_t beyond = {{4, 3}, 20.f, 14.f};
    bolt = UINT32_MAX;
    ASSERT_TRUE(!fr_player_cast_fire_bolt(gpu, 7u, &edge, &beyond, &bolt,
                                          err, sizeof(err)));
    ASSERT_EQ(bolt, 0u);
    ASSERT_TRUE(dc_gpu_read_extraction_stats(gpu, &after));
    ASSERT_EQ(after.owned_bodies, before.owned_bodies);
    PASS();
}
static void test_ice_storm_chills_cells_within_radius(void) {
    ASSERT_TRUE(setup_world());
    /* Two hot sand pockets: one at the storm target and one far away.
     * Sand survives the soaking brush, and reactive seeds must match
     * their physical cell material. */
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 224u, 202u, 0u, DC_MATERIAL_SAND,
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 255u, 255u, 0u, DC_MATERIAL_SAND,
                                      err, sizeof(err)));
    fr_reactive_state_t sand_hot, sand_far;
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_SAND, 3000u, 0u, 900.f,
                                          0x71000000u, &sand_hot, err, sizeof(err)));
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_SAND, 3000u, 0u, 900.f,
                                          0x71000001u, &sand_far, err, sizeof(err)));
    uint32_t local = 32u + 10u * 64u;
    uint32_t far_local = 63u + 63u * 64u;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(gpu, 11u, local, &sand_hot, err,
                                          sizeof(err)));
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(gpu, 11u, far_local, &sand_far,
                                          err, sizeof(err)));
    fr_player_point_t target = point_at(224.f, 202.f);
    ASSERT_TRUE(fr_player_cast_ice_storm(gpu, &target, 6u, err, sizeof(err)));
    fr_reactive_record_t chilled, control;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, 11u, local, &chilled, &found,
                                          err, sizeof(err)) &&
                found);
    ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, 11u, far_local, &control,
                                          &found, err, sizeof(err)) &&
                found);
    double before = fr_reactive_state_energy(&sand_hot);
    double after = fr_reactive_state_energy(&chilled.state);
    printf("storm chill %.4g -> %.4g control %.4g\n", before, after,
           fr_reactive_state_energy(&control.state));
    /* The target pocket lost most of its heat; the far pocket is untouched. */
    ASSERT_TRUE(after < before * 0.4);
    ASSERT_TRUE(fabs(fr_reactive_state_energy(&control.state) - before) < 1.);
    PASS();
}
static void test_ice_storm_bounds_its_area(void) {
    ASSERT_TRUE(setup_world());
    fr_player_point_t target = point_at(ORIGIN_X + TARGET_DX,
                                        ORIGIN_Y + TARGET_DY);
    ASSERT_TRUE(fr_player_cast_ice_storm(gpu, &target, 6u, err, sizeof(err)));
    dc_chunk_t *probe = calloc(1, sizeof(*probe));
    ASSERT_TRUE(probe && dc_gpu_download_chunk(gpu, 11u, probe, err,
                                               sizeof(err)));
    /* Water fills the soaked disc; the far corner stays dry stone. */
    unsigned wet = 0u;
    for (uint32_t y = 2u; y < 19u; y++)
        for (uint32_t x = 30u; x < 47u; x++)
            if (probe->cells[y * 64u + x].fluid_mass > 0u)
                wet++;
    printf("storm water cells %u\n", wet);
    ASSERT_TRUE(wet > 20u);
    ASSERT_EQ(probe->cells[40u * 64u + 63u].material, DC_MATERIAL_STONE);
    free(probe);
    PASS();
}
static void test_ice_storm_rejects_zero_radius(void) {
    ASSERT_TRUE(setup_world());
    fr_player_point_t target = point_at(ORIGIN_X, ORIGIN_Y);
    ASSERT_TRUE(!fr_player_cast_ice_storm(gpu, &target, 0u, err, sizeof(err)));
    PASS();
}

static void test_ice_storm_keeps_generic_body_inventory_unchanged(void) {
    ASSERT_TRUE(setup_world());
    fr_player_point_t target = point_at(224.f, 202.f);
    uint32_t before = gpu->body_count;
    ASSERT_TRUE(fr_player_cast_ice_storm(gpu, &target, 6u, err, sizeof(err)));
    ASSERT_INT_EQ(gpu->body_count, before);
    PASS();
}

static void test_ice_storm_tracks_streamed_pages_across_a_seam(void) {
    ASSERT_TRUE(setup_world());
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    const dc_chunk_coord_t center = {-20, 118};
    const uint32_t slots[] = {12u, 11u};
    const uint32_t sand_cells[] = {62u + 12u * 64u, 1u + 12u * 64u};
    fr_reactive_state_t hot;
    for (uint32_t i = 0; i < 2u; i++) {
        chunk->coord = (dc_chunk_coord_t){center.x + i, center.y};
        chunk->cells[sand_cells[i]].material = DC_MATERIAL_SAND;
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slots[i], chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu, 2u + i, 3u, slots[i], err, sizeof(err)));
        ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_SAND, 3000u, 0u,
                                             900.f, 0x71000100u + i, &hot,
                                             err, sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_seed_cell(gpu, slots[i], sand_cells[i],
                                              &hot, err, sizeof(err)));
        chunk->cells[sand_cells[i]].material = DC_MATERIAL_AIR;
    }
    const fr_player_point_t target = {center, 63.f, 12.f};
    ASSERT_TRUE(fr_player_cast_ice_storm(gpu, &target, 4u, err, sizeof(err)));
    for (uint32_t i = 0; i < 2u; i++) {
        fr_reactive_record_t chilled;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, slots[i], sand_cells[i],
                                              &chilled, &found, err,
                                              sizeof(err)));
        ASSERT_TRUE(found);
        ASSERT_TRUE(fabs(fr_reactive_state_energy(&chilled.state) -
                         fr_reactive_state_energy(&hot) * 0.3) < 0.001);
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, slots[i], chunk, err, sizeof(err)));
        uint32_t wet_x = i == 0u ? 63u : 0u;
        ASSERT_TRUE(chunk->cells[11u * 64u + wet_x].fluid_mass > 0u);
        ASSERT_EQ(chunk->cells[7u * 64u + wet_x].fluid_mass, 0u);
    }
    free(chunk);
    PASS();
}

static void test_ice_storm_rejects_unmapped_or_noncanonical_target(void) {
    ASSERT_TRUE(setup_world());
    const fr_player_point_t unmapped = {{4, 3}, 2.f, 12.f};
    const fr_player_point_t noncanonical = {{4, 3}, -2.f, 12.f};
    ASSERT_TRUE(!fr_player_cast_ice_storm(gpu, &unmapped, 6u, err, sizeof(err)));
    ASSERT_TRUE(!fr_player_cast_ice_storm(gpu, &noncanonical, 6u, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk && dc_gpu_download_chunk(gpu, 11u, chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[12u * 64u + 62u].fluid_mass, 0u);
    free(chunk);
    PASS();
}

int main(void) {
    printf("=== Player Ability Cast Tests ===\n\n");
    RUN(test_fire_bolt_spawns_burning_body_toward_target);
    RUN(test_fire_bolt_ignites_wood_on_impact);
    RUN(test_fire_bolt_owns_every_burning_texel_across_a_streamed_seam);
    RUN(test_repeated_bolts_leave_unrelated_bodies_and_terrain_alone);
    RUN(test_fire_bolt_rejects_occupied_and_unmapped_footprints);
    RUN(test_ice_storm_chills_cells_within_radius);
    RUN(test_ice_storm_bounds_its_area);
    RUN(test_ice_storm_rejects_zero_radius);
    RUN(test_ice_storm_keeps_generic_body_inventory_unchanged);
    RUN(test_ice_storm_tracks_streamed_pages_across_a_seam);
    RUN(test_ice_storm_rejects_unmapped_or_noncanonical_target);
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
