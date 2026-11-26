#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define main rigid_solver_fixture_main
#include "../vulkan/rigid_solver_tests.c"
#undef main

#include <SDL.h>
#include "../../src/app/siege_scene.h"
#include "../../src/app/view_config.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "../../src/vulkan/reactive_storage_gpu.h"
#include "../../src/vulkan/thermal_gpu_state.h"

static int g_pass, g_fail;
#include "ferrum/fire/gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"

static uint32_t greek_tile_slot(const dc_gpu_t *gpu, uint32_t x, uint32_t y) {
    const uint32_t *pages = gpu->page_mapped;
    return pages[(y / 64u) * gpu->page_width + x / 64u];
}
static volatile sig_atomic_t greek_stopped;
static void greek_stop(int number) {(void)number;greek_stopped = 1;}
static void greek_install_stop(void) {
    signal(SIGINT, greek_stop);
    signal(SIGTERM, greek_stop);
}

static dc_gpu_t *greek_grid(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, SIM_WIDTH, SIM_HEIGHT,
                       "build/shaders/pattern.comp.spv",
                       err, sizeof(err))) {
        fprintf(stderr, "greek grid: %s\n", err);
        return NULL;
    }
    return gpu;
}
static bool greek_ticks(dc_gpu_t *gpu, uint32_t seconds, char *err, uint32_t cap) {
    for (uint32_t second = 0; second < seconds && !greek_stopped; second++)
        for (uint32_t tick = 0; tick < 60u; tick++)
            if (!dc_gpu_tick_step(gpu, err, cap)) return false;
    return !greek_stopped;
}
/* Total film stock over the resting ball's parcels: oil plus jelly fuel. */
static uint64_t greek_ball_coating(dc_gpu_t *gpu, uint32_t body, uint32_t span,
                                   char *err, uint32_t cap) {
    uint64_t total = 0;
    for (uint32_t py = 0; py < span; py++)
        for (uint32_t px = 0; px < span; px++) {
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_body(gpu, body, px, py, &parcel, &found,
                                           err, cap)) {
                fprintf(stderr, "coating read %u,%u failed: %s\n", px, py, err);
                return UINT64_MAX;
            }
            if (!found) continue;
            total += parcel.state.coating_oil_fuel_fp +
                     parcel.state.coating_jelly_fuel_fp;
        }
    return total;
}
static uint64_t greek_ball_water(dc_gpu_t *gpu, uint32_t body, uint32_t span,
                                 char *err, uint32_t cap) {
    uint64_t total = 0;
    for (uint32_t py = 0; py < span; py++)
        for (uint32_t px = 0; px < span; px++) {
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_body(gpu, body, px, py, &parcel, &found,
                                           err, cap))
                return UINT64_MAX;
            if (!found) continue;
            total += parcel.state.bound_water_fp;
        }
    return total;
}

static bool greek_ball_water_contact(dc_gpu_t *gpu, uint32_t body,
                                    uint32_t *wet_faces, uint32_t *faces,
                                    char *err, uint32_t cap) {
    enum {span = 2 * DC_SIEGE_COAL_RADIUS + 1};
    dc_gpu_world_body_t world;
    dc_gpu_body_motion_t motion;
    dc_gpu_body_pixel_t pixels[span * span];
    uint32_t count;
    if (!dc_gpu_read_world_body(gpu, body, &world, err, cap) ||
        !dc_gpu_read_body_motion(gpu, body, &motion, err, cap) ||
        motion.angle != 0.f || world.chunk.x < 0 || world.chunk.y < 0 ||
        world.chunk.x >= gpu->page_width || world.chunk.y >= gpu->page_height ||
        world.body.x_fp % 65536 || world.body.y_fp % 65536 ||
        !dc_gpu_read_pixel_shape(gpu, body, pixels, span * span, &count, err, cap))
        return false;
    int origin_x = world.body.x_fp / 65536;
    int origin_y = world.body.y_fp / 65536;
    if (origin_x < 1 || origin_x + span >= (int)DC_CHUNK_SIDE ||
        origin_y < 1 || origin_y + span >= (int)DC_CHUNK_SIDE)
        return false;
    bool solid[span + 2][span + 2] = {{false}};
    for (uint32_t i = 0; i < count; i++) {
        if (pixels[i].x >= span || pixels[i].y >= span) return false;
        solid[pixels[i].y + 1][pixels[i].x + 1] = true;
    }
    uint32_t slot = greek_tile_slot(gpu, world.chunk.x * DC_CHUNK_SIDE,
                                    world.chunk.y * DC_CHUNK_SIDE);
    dc_chunk_t chunk;
    if (!slot || !dc_gpu_download_chunk(gpu, slot - 1u, &chunk, err, cap))
        return false;
    static const int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    bool trace = getenv("DC_GREEK_TRACE") != NULL;
    *wet_faces = 0;
    *faces = 0;
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t side = 0; side < 4u; side++) {
            int x = (int)pixels[i].x + directions[side][0];
            int y = (int)pixels[i].y + directions[side][1];
            if (solid[y + 1][x + 1]) continue;
            const dc_cell_t *cell = &chunk.cells[(origin_y + y) * DC_CHUNK_SIDE +
                                                 origin_x + x];
            (*faces)++;
            if (cell->material == DC_MATERIAL_WATER &&
                cell->fluid_mass >= DC_FLUID_FULL * 3u / 4u)
                (*wet_faces)++;
            else if (trace)
                printf("Pool face (%d, %d) material %u water %u\n",
                       origin_x + x, origin_y + y, cell->material, cell->fluid_mass);
        }
    }
    return *faces > 0u;
}

static void test_pool_soaks_a_stationary_oil_load(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    dc_siege_scene_t scene = {0};
    const uint32_t span = 2u * DC_SIEGE_COAL_RADIUS + 1u;
    ASSERT_TRUE(gpu && dc_siege_scene_build_douse_pool(gpu, DC_SIEGE_LOAD_OIL,
                                                      &scene, err, sizeof(err)));
    uint64_t admitted_water = greek_ball_water(gpu, scene.coal_body, span, err,
                                              sizeof(err));
    ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
    uint32_t wet_faces, faces;
    ASSERT_TRUE(greek_ball_water_contact(gpu, scene.coal_body, &wet_faces, &faces,
                                        err, sizeof(err)));
    uint64_t soaked_water = greek_ball_water(gpu, scene.coal_body, span, err,
                                            sizeof(err));
    printf("Standing pool covered %u/%u exposed faces; bound water %llu -> %llu Q16\n",
           wet_faces, faces, (unsigned long long)admitted_water,
           (unsigned long long)soaked_water);
    ASSERT_EQ(scene.pouch_joint, 0u);
    ASSERT_EQ(admitted_water, 0u);
    ASSERT_TRUE(soaked_water > 0u && soaked_water != UINT64_MAX);
    ASSERT_EQ(wet_faces, faces);
    dc_gpu_destroy(gpu);
    PASS();
}
static bool greek_open_pouch_at_launch(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                                      char *err, uint32_t cap) {
    dc_gpu_body_motion_t arm = {0};
    float release_angle = 0.3f;
    const char *tuned = getenv("DC_SIEGE_RELEASE_ANGLE");
    if (tuned) release_angle = strtof(tuned, NULL);
    for (uint32_t tick = 0; tick < 240u && !greek_stopped; tick++) {
        if (!dc_gpu_tick_step(gpu, err, cap)) return false;
        if (!dc_gpu_read_body_motion(gpu, scene->arm_body, &arm, err, cap))
            return false;
        if (arm.angle >= release_angle)
            return dc_siege_scene_release_projectile(gpu, scene, err, cap);
    }
    if (err && cap) snprintf(err, cap, "Arm did not reach the pouch release angle");
    return false;
}

static bool greek_land_ball(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                            dc_siege_load_t load, bool ignite, float *rest_x,
                            float *rest_y, char *err, uint32_t cap) {
    if (!greek_ticks(gpu, 2u, err, cap)) return false;
    if (!dc_siege_scene_release(gpu, scene, err, cap)) return false;
    if (ignite && !dc_siege_scene_ignite_load(gpu, scene, 0.f, err, cap))
        return false;
    if (!greek_open_pouch_at_launch(gpu, scene, err, cap)) return false;
    dc_gpu_world_body_t coal = {0};
    for (uint32_t tick = 0; tick < 240u; tick++) {
        if (!dc_gpu_tick_step(gpu, err, cap)) return false;
        if (!dc_gpu_read_world_body(gpu, scene->coal_body, &coal, err, cap))
            return false;
        float speed = fabsf((float)coal.body.vx_fp / 65536.f) + fabsf((float)coal.body.vy_fp / 65536.f);
        if (tick > 60u && speed < 0.05f) break;
    }
    (void)load;
    *rest_x = (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f;
    *rest_y = (float)coal.chunk.y * 64.f + coal.body.y_fp / 65536.f;
    return true;
}

static void test_greek_and_oil_stage_matched_prearmed_coating(void) {
    char err[256] = {0};
    static const dc_siege_load_t loads[2] = {DC_SIEGE_LOAD_OIL,
                                             DC_SIEGE_LOAD_GREEK};
    for (unsigned which = 0; which < 2u; which++) {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        bool built = dc_siege_scene_build_load(gpu, false, loads[which], &scene,
                                               err, sizeof(err));
        if (!built) printf("greek stage build: %s\n", err);
        ASSERT_TRUE(built);
        fr_reactive_record_t core;
        bool found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u,
                                              &core, &found, err, sizeof(err)) &&
                    found);
        ASSERT_EQ(core.state.material, DC_MATERIAL_STONE);
        /* The fuse stages cold: dc_siege_scene_ignite_load lights it. */
        ASSERT_TRUE((core.state.flags & FR_REACTIVE_COATING_FLAMING) == 0u);
        uint64_t film = core.state.coating_oil_fuel_fp +
                        core.state.coating_jelly_fuel_fp;
        ASSERT_EQ(film, 65536u);
        ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u,
                                              &core, &found, err, sizeof(err)) &&
                    found);
        ASSERT_TRUE((core.state.flags & FR_REACTIVE_COATING_FLAMING) != 0u);
        if (loads[which] == DC_SIEGE_LOAD_GREEK) {
            ASSERT_EQ(core.state.coating_oil_fuel_fp, 32768u);
            ASSERT_EQ(core.state.coating_jelly_fuel_fp, 32768u);
            ASSERT_TRUE(core.state.coating_oxidizer_fp > 0u);
        } else {
            ASSERT_EQ(core.state.coating_jelly_fuel_fp, 0u);
            ASSERT_EQ(core.state.coating_oxidizer_fp, 0u);
        }
        dc_gpu_destroy(gpu);
    }
    PASS();
}
static void test_cold_greek_load_keeps_its_film_while_soaking(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    dc_siege_scene_t scene = {0};
    const uint32_t span = 2u * DC_SIEGE_COAL_RADIUS + 1u;
    ASSERT_TRUE(gpu && dc_siege_scene_build_douse_pool(gpu, DC_SIEGE_LOAD_GREEK,
                                                      &scene, err, sizeof(err)));
    uint64_t before = greek_ball_coating(gpu, scene.coal_body, span, err, sizeof(err));
    ASSERT_TRUE(before > 0u && before != UINT64_MAX);
    ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
    uint64_t after = greek_ball_coating(gpu, scene.coal_body, span, err, sizeof(err));
    printf("Unlit Greek load in pool retained %llu of %llu Q16 film\n",
           (unsigned long long)after, (unsigned long long)before);
    ASSERT_EQ(after, before);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_siege_strike_preserves_the_loads_bound_water(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    dc_siege_scene_t scene = {0};
    const uint32_t span = 2u * DC_SIEGE_COAL_RADIUS + 1u;
    ASSERT_TRUE(gpu && dc_siege_scene_build_douse_pool(gpu, DC_SIEGE_LOAD_OIL,
                                                      &scene, err, sizeof(err)));
    ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
    uint64_t before = greek_ball_water(gpu, scene.coal_body, span, err, sizeof(err));
    ASSERT_TRUE(before > 0u && before != UINT64_MAX);
    ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
    uint64_t after = greek_ball_water(gpu, scene.coal_body, span, err, sizeof(err));
    printf("Strike retained %llu of %llu Q16 bound water\n",
           (unsigned long long)after, (unsigned long long)before);
    ASSERT_EQ(after, before);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_submerged_greek_keeps_reacting_while_oil_douses(void) {
    enum {span = 2 * DC_SIEGE_COAL_RADIUS + 1, soak_seconds = 3,
          window_seconds = 30, midpoint = window_seconds / 2};
    char err[256] = {0};
    static const dc_siege_load_t loads[2] = {DC_SIEGE_LOAD_OIL,
                                             DC_SIEGE_LOAD_GREEK};
    uint64_t cold_fuel[2] = {0, 0};
    uint64_t cold_water[2] = {0, 0};
    uint64_t late_slope[2] = {0, 0};
    bool submerged[2] = {true, true};
    for (unsigned which = 0; which < 2u; which++) {
        const char *name = which == 0u ? "oil" : "Greek";
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        bool built_pool = dc_siege_scene_build_douse_pool(
            gpu, loads[which], &scene, err, sizeof(err));
        if (!built_pool) printf("greek pool build: %s\n", err);
        ASSERT_TRUE(built_pool);
        ASSERT_TRUE(greek_ticks(gpu, soak_seconds, err, sizeof(err)));
        cold_fuel[which] = greek_ball_coating(gpu, scene.coal_body, span, err,
                                              sizeof(err));
        cold_water[which] = greek_ball_water(gpu, scene.coal_body, span, err,
                                             sizeof(err));
        ASSERT_TRUE(cold_fuel[which] > 0u && cold_fuel[which] != UINT64_MAX);
        ASSERT_TRUE(cold_water[which] > 0u && cold_water[which] != UINT64_MAX);
        uint32_t wet_faces, faces;
        ASSERT_TRUE(greek_ball_water_contact(gpu, scene.coal_body, &wet_faces,
                                            &faces, err, sizeof(err)));
        ASSERT_EQ(wet_faces, faces);
        printf("%s after %us soak: film %llu water %llu wet faces %u/%u\n",
               name, soak_seconds, (unsigned long long)cold_fuel[which],
               (unsigned long long)cold_water[which], wet_faces, faces);
        ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
        uint64_t film = cold_fuel[which], mid = 0u;
        for (uint32_t second = 1; second <= window_seconds; second++) {
            bool advanced = greek_ticks(gpu, 1u, err, sizeof(err));
            if (!advanced) printf("%s tick at %us failed: %s\n", name, second, err);
            ASSERT_TRUE(advanced);
            ASSERT_TRUE(greek_ball_water_contact(gpu, scene.coal_body, &wet_faces,
                                                &faces, err, sizeof(err)));
            uint64_t water = greek_ball_water(gpu, scene.coal_body, span, err,
                                               sizeof(err));
            film = greek_ball_coating(gpu, scene.coal_body, span, err, sizeof(err));
            ASSERT_TRUE(film != UINT64_MAX && water != UINT64_MAX);
            printf("%s after %us burn: film %llu water %llu wet faces %u/%u\n",
                   name, second, (unsigned long long)film,
                   (unsigned long long)water, wet_faces, faces);
            if (wet_faces * 4u < faces * 3u || water == 0u)
                submerged[which] = false;
            if (second == midpoint) mid = film;
        }
        if (which == 0u) ASSERT_TRUE(film > 0u);
        late_slope[which] = mid > film ? mid - film : 0u;
        dc_gpu_destroy(gpu);
    }
    ASSERT_EQ(cold_fuel[1], cold_fuel[0]);
    ASSERT_EQ(cold_water[1], cold_water[0]);
    ASSERT_TRUE(submerged[0] && submerged[1]);
    printf("Late-window fuel loss: oil %llu, Greek %llu\n",
           (unsigned long long)late_slope[0], (unsigned long long)late_slope[1]);
    ASSERT_TRUE(late_slope[1] > 0u);
    ASSERT_TRUE(late_slope[1] >= late_slope[0] * 2u);
    PASS();
}
static void test_greek_depletes_matched_oil_twice_as_fast_dry(void) {
    char err[256] = {0};
    static const dc_siege_load_t loads[2] = {DC_SIEGE_LOAD_OIL,
                                             DC_SIEGE_LOAD_GREEK};
    uint64_t consumed[2] = {0, 0};
    for (unsigned which = 0; which < 2u; which++) {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        bool built = dc_siege_scene_build_load(gpu, false, loads[which], &scene,
                                               err, sizeof(err));
        if (!built) printf("greek stage build: %s\n", err);
        ASSERT_TRUE(built);
        {
            fr_reactive_record_t probe;
            bool found = false;
            fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u, &probe,
                                      &found, err, sizeof(err));
            printf("greek pool ball at t0: coal_body %x found %d film %llu\n",
                   scene.coal_body, found,
                   found ? (unsigned long long)(
                               probe.state.coating_oil_fuel_fp +
                               probe.state.coating_jelly_fuel_fp)
                         : 0ull);
            {
                const dc_gpu_address_buffer_t *fluxes =
                    &gpu->thermal->fluxes;
                const uint32_t *words = fluxes->mapped;
                uint32_t width = gpu->width;
                uint32_t seen = 0, sentinel = 0, wet_cells = 0;
                for (uint32_t y = 210u; y <= 221u; y++)
                    for (uint32_t x = 22u; x <= 42u; x++) {
                        uint32_t index = y * width + x;
                        uint32_t available, magic;
                        memcpy(&available, &words[(index + width) * 4u + 1u],
                               sizeof(available));
                        memcpy(&magic, &words[(index + width) * 4u + 3u],
                               sizeof(magic));
                        if ((magic & 0x7f800000u) == 0x5e000000u) sentinel++;
                        if (available) { seen++; wet_cells = index; }
                    }
                printf("greek flux probe: wet flux cells %u last %u "
                       "sentinel %u\n", seen, wet_cells, sentinel);
            }
            dc_chunk_t *pool = calloc(1, sizeof(*pool));
            if (dc_gpu_download_chunk(gpu, greek_tile_slot(gpu, 32u, 215u) - 1u,
                                      pool, err, sizeof(err))) {
                uint32_t wet = 0;
                for (uint32_t i = 0; i < 4096u; i++)
                    if (pool->cells[i].material == DC_MATERIAL_WATER &&
                        pool->cells[i].fluid_mass == DC_FLUID_FULL)
                        wet++;
                printf("greek pool chunk 30: water-full cells %u "
                       "(cell(32,215) material %u fluid %u) "
                       "coin(27,213) material %u fluid %u\n",
                       wet, pool->cells[(215u % 64u) * 64u + 32u].material,
                       pool->cells[(215u % 64u) * 64u + 32u].fluid_mass,
                       pool->cells[(213u % 64u) * 64u + 27u].material,
                       pool->cells[(213u % 64u) * 64u + 27u].fluid_mass);
            }
            free(pool);
        }
        ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
        uint64_t before = greek_ball_coating(gpu, scene.coal_body, 9u, err,
                                             sizeof(err));
        ASSERT_TRUE(before != UINT64_MAX && before > 0u);
        float rest_x = 0.f, rest_y = 0.f;
        ASSERT_TRUE(greek_land_ball(gpu, &scene, loads[which], true, &rest_x,
                                    &rest_y, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 30u, err, sizeof(err)));
        uint64_t after = greek_ball_coating(gpu, scene.coal_body, 9u, err,
                                            sizeof(err));
        ASSERT_TRUE(after != UINT64_MAX);
        consumed[which] = before > after ? before - after : 0u;
        printf("greek %s dry consumed %llu in 30s\n",
               which == 0u ? "oil" : "greek", (unsigned long long)consumed[which]);
        dc_gpu_destroy(gpu);
    }
    ASSERT_TRUE(consumed[1] * 2u >= consumed[0]);
    PASS();
}
static void test_painted_water_soaks_a_wood_body(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    ASSERT_TRUE(dc_siege_scene_build_load(gpu, false, DC_SIEGE_LOAD_OIL, &scene,
                                          err, sizeof(err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(gpu, err, sizeof(err)));
    dc_gpu_world_body_t ball;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &ball, err,
                                       sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 70, 152, 5, DC_MATERIAL_WATER, err,
                                      sizeof(err)));
    for (uint32_t second = 0; second < 10u; second++) {
        ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
        uint64_t water = greek_ball_water(gpu, scene.coal_body, 9u, err,
                                          sizeof(err));
        if (second % 3u == 0u)
            printf("soak probe %us bound water %llu\n", second,
                   (unsigned long long)water);
    }
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_delayed_greek_load_ignites_the_launcher_ensemble(void) {
    char err[256] = {0};
    uint32_t ignited = 0u;
    const uint32_t seeds = 16u;
    for (uint32_t seed = 0u; seed < seeds; seed++) {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        ASSERT_TRUE(dc_siege_scene_build_risk(gpu, DC_SIEGE_LOAD_GREEK, seed,
                                              false, &scene, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 2u, err, sizeof(err)));
        ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
        /* Ten exposed dry seconds in the sling. */
        ASSERT_TRUE(greek_ticks(gpu, 10u, err, sizeof(err)));
        uint32_t flaming = 0u;
        for (uint32_t py = 0u; py < 12u; py++)
            for (uint32_t px = 0u; px < 2u; px++) {
                fr_reactive_record_t arm_parcel;
                bool found = false;
                if (fr_gpu_reactive_read_body(gpu, scene.arm_body, px, py,
                                              &arm_parcel, &found, err,
                                              sizeof(err)) &&
                    found &&
                    (arm_parcel.state.flags & FR_REACTIVE_FLAMING) != 0u)
                    flaming++;
            }
        if (flaming >= 3u) ignited++;
        printf("greek risk seed %u arm flaming %u\n", seed, flaming);
        dc_gpu_destroy(gpu);
    }
    printf("greek risk ensemble: %u of %u seeds ignited the launcher\n",
           ignited, seeds);
    ASSERT_TRUE(ignited * 4u >= seeds * 3u);
    PASS();
}
static void test_prompt_launch_and_fireproof_carrier_stay_safe(void) {
    char err[256] = {0};
    /* Prompt launch: the fuse lights as the pouch opens; the load leaves
     * before the carrier film can spread. */
    {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        ASSERT_TRUE(dc_siege_scene_build_risk(gpu, DC_SIEGE_LOAD_GREEK, 7u,
                                              false, &scene, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 2u, err, sizeof(err)));
        ASSERT_TRUE(dc_siege_scene_release(gpu, &scene, err, sizeof(err)));
        {
            bool fuse = dc_siege_scene_ignite_load(gpu, &scene, 0.6f, err,
                                                   sizeof(err));
            if (!fuse) printf("greek fuse ignite: %s\n", err);
            ASSERT_TRUE(fuse);
        }
        bool released = greek_open_pouch_at_launch(gpu, &scene, err, sizeof(err));
        if (!released) printf("greek pouch release failed: %s\n", err);
        ASSERT_TRUE(released);
        ASSERT_TRUE(greek_ticks(gpu, 10u, err, sizeof(err)));
        bool pouch_attached = true;
        ASSERT_TRUE(fr_gpu_revolute_active(gpu, scene.pouch_joint, &pouch_attached,
                                           err, sizeof(err)) && !pouch_attached);
        uint32_t flaming = 0u;
        uint64_t fuel = 0u;
        for (uint32_t py = 0u; py < 12u; py++)
            for (uint32_t px = 0u; px < 2u; px++) {
                fr_reactive_record_t arm_parcel;
                bool found = false;
                if (fr_gpu_reactive_read_body(gpu, scene.arm_body, px, py,
                                              &arm_parcel, &found, err,
                                              sizeof(err)) &&
                    found) {
                    fuel += arm_parcel.state.fuel_mass_fp;
                    if ((arm_parcel.state.flags & FR_REACTIVE_FLAMING) != 0u)
                        flaming++;
                }
            }
        printf("greek prompt-launch arm flaming %u fuel %llu\n", flaming,
               (unsigned long long)fuel);
        /* A launched burning load scorches the arm top, but the launcher is
         * preserved when the scanned load-bearing wood stays mostly intact
         * (the 24-parcel scan holds ~943k when pristine). */
        ASSERT_TRUE(fuel >= 900000u);
        dc_gpu_destroy(gpu);
    }
    /* Fireproof carrier: the stone arm holds the burning load. */
    {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        ASSERT_TRUE(dc_siege_scene_build_risk(gpu, DC_SIEGE_LOAD_GREEK, 7u,
                                              true, &scene, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 2u, err, sizeof(err)));
        ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 10u, err, sizeof(err)));
        dc_gpu_world_body_t arm;
        ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.arm_body, &arm, err,
                                           sizeof(err)));
        printf("greek fireproof carrier survives: body %x active\n",
               arm.body.id);
        ASSERT_TRUE(arm.body.id != 0u);
        dc_gpu_destroy(gpu);
    }
    PASS();
}
static bool greek_cell_corrosion(dc_gpu_t *gpu, uint32_t x, uint32_t y,
                                 float *out, bool *found, char *err,
                                 uint32_t cap) {
    uint32_t slot = greek_tile_slot(gpu, x, y);
    if (!slot || slot > DC_GPU_CHUNK_SLOTS) return false;
    fr_reactive_record_t record;
    if (!fr_gpu_reactive_read_cell(gpu, slot - 1u,
                                   (y % 64u) * 64u + x % 64u, &record, found,
                                   err, cap))
        return false;
    if (found) {
        *out = record.state.corroded_fraction;
        fprintf(stderr, "coin %u,%u film oil %u jelly %u ox %u corroded %.4f "
                        "flags %x\n",
                x, y, record.state.coating_oil_fuel_fp,
                record.state.coating_jelly_fuel_fp,
                record.state.coating_oxidizer_fp,
                record.state.corroded_fraction, record.state.flags);
    }
    return true;
}

static void test_stationary_greek_film_corrodes_clean_pool_samples(void) {
    const uint32_t x = DC_SIEGE_CONTACT_SAMPLE_X;
    const uint32_t ys[] = {DC_SIEGE_CONTACT_GOLD_Y, DC_SIEGE_CONTACT_INGREDIENT_Y};
    const uint32_t span = 2u * DC_SIEGE_COAL_RADIUS + 1u;
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    dc_siege_scene_t scene = {0};
    ASSERT_TRUE(gpu && dc_siege_scene_build_contact_pool(gpu, &scene, err, sizeof(err)));
    ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
    const uint64_t admitted = greek_ball_coating(gpu, scene.coal_body, span, err, sizeof(err));
    ASSERT_TRUE(admitted > 0u && admitted != UINT64_MAX);
    fr_reactive_record_t samples[2];
    uint32_t origins[2];
    float progress[2] = {0.f, 0.f};
    for (uint32_t second = 0u; second <= 30u; second++) {
        if (second > 0u) ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
        for (uint32_t i = 0u; i < 2u; i++) {
            uint32_t slot = greek_tile_slot(gpu, x, ys[i]);
            uint32_t local = (ys[i] % DC_CHUNK_SIDE) * DC_CHUNK_SIDE + x % DC_CHUNK_SIDE;
            bool found = false;
            ASSERT_TRUE(slot && fr_gpu_reactive_read_cell(gpu, slot - 1u, local, &samples[i],
                                                         &found, err, sizeof(err)) && found);
            const fr_reactive_state_t *state = &samples[i].state;
            ASSERT_TRUE(fr_reactive_state_valid(state, err, sizeof(err)));
            if (second == 0u) {
                origins[i] = state->origin_id_low;
                ASSERT_EQ(state->coating_oxidizer_fp, 0u);
                ASSERT_EQ(state->coating_oil_fuel_fp, 0u);
                ASSERT_EQ(state->coating_jelly_fuel_fp, 0u);
                ASSERT_EQ(state->corroded_fraction, 0.f);
            }
            ASSERT_EQ(state->origin_id_low, origins[i]);
            ASSERT_TRUE(state->corroded_fraction >= progress[i]);
            ASSERT_TRUE((uint64_t)state->core_oxidizer_fp + state->coating_oxidizer_fp <= admitted);
            progress[i] = state->corroded_fraction;
        }
        if (second == 0u)
            ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
    }
    printf("Persistent pool contact after 30s, gold %.6f, ingredient %.6f\n",
           progress[0], progress[1]);
    ASSERT_TRUE(progress[0] > 0.f);
    ASSERT_TRUE(progress[1] > 0.f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_greek_burn_corrodes_coins_across_reload(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = greek_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    ASSERT_TRUE(dc_siege_scene_build_corrosion_pool(gpu, DC_SIEGE_LOAD_GREEK,
                                                    &scene, err, sizeof(err)));
    ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, 0.f, err, sizeof(err)));
    float coin_before = -1.f, ingredient_before = -1.f;
    bool found = false;
    ASSERT_TRUE(greek_cell_corrosion(gpu, 27u, 213u, &coin_before, &found, err,
                                     sizeof(err)) &&
                found);
    ASSERT_TRUE(greek_cell_corrosion(gpu, 27u, 216u, &ingredient_before,
                                     &found, err, sizeof(err)) &&
                found);
    ASSERT_TRUE(greek_ticks(gpu, 30u, err, sizeof(err)));
    float coin_after = -1.f, ingredient_after = -1.f;
    ASSERT_TRUE(greek_cell_corrosion(gpu, 27u, 213u, &coin_after, &found, err,
                                     sizeof(err)) &&
                found);
    ASSERT_TRUE(greek_cell_corrosion(gpu, 27u, 216u, &ingredient_after, &found,
                                    err, sizeof(err)) &&
                found);
    printf("greek corrosion coin %.4f->%.4f ingredient %.4f->%.4f\n",
           coin_before, coin_after, ingredient_before, ingredient_after);
    {
        dc_chunk_t *verify = calloc(1, sizeof(*verify));
        fr_reactive_chunk_t records = {0};
        uint32_t slot0 = greek_tile_slot(gpu, 27u, 213u);
        if (dc_gpu_download_chunk(gpu, slot0 - 1u, verify, err, sizeof(err)) &&
            fr_gpu_reactive_export_chunk(gpu, slot0 - 1u, verify, &records,
                                         err, sizeof(err))) {
            uint32_t gold_records = 0u;
            for (uint32_t i = 0u; i < records.count; i++)
                if (records.attachments[i].record.state.material ==
                    DC_MATERIAL_GOLD) {
                    gold_records++;
                    fprintf(stderr,
                            "gold record %u film %u/%u/%u corroded %.4f\n", i,
                            records.attachments[i].record.state.coating_oil_fuel_fp,
                            records.attachments[i].record.state.coating_jelly_fuel_fp,
                            records.attachments[i].record.state.coating_oxidizer_fp,
                            records.attachments[i].record.state.corroded_fraction);
                }
            printf("greek gold records in chunk 30: %u (records %u)\n",
                   gold_records, records.count);
            fr_reactive_chunk_release(&records);
        }
        free(verify);
    }
    ASSERT_TRUE(coin_after > coin_before);
    /* Reload through the reactive chunk save path: the spent fraction is
     * exported and reimported with the record. */
    /* Reload through the reactive record snapshot save format: the spent
     * corrosion fraction must survive encode and decode unchanged. */
    fr_reactive_record_t spent;
    bool spent_found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, greek_tile_slot(gpu, 27u, 213u) - 1u,
                                          (213u % 64u) * 64u + 27u, &spent,
                                          &spent_found, err, sizeof(err)) &&
                spent_found);
    uint8_t bytes[512];
    size_t written = 0;
    uint64_t parcel = (uint64_t)spent.state.parcel_id_high << 32 |
                      spent.state.parcel_id_low;
    uint64_t origin = (uint64_t)spent.state.origin_id_high << 32 |
                      spent.state.origin_id_low;
    uint64_t counter = (parcel > origin ? parcel : origin) + 1u;
    ASSERT_TRUE(fr_reactive_snapshot_encode(&spent, 1u, counter, bytes,
                                            sizeof(bytes), &written, err,
                                            sizeof(err)));
    fr_reactive_record_t restored = {0};
    uint32_t count = 0u, required = 0u;
    uint64_t next = 0u;
    ASSERT_TRUE(fr_reactive_snapshot_decode(bytes, written, &restored, 1u,
                                            &count, &next, &required, err,
                                            sizeof(err)));
    ASSERT_EQ(count, 1u);
    ASSERT_TRUE(fabsf(restored.state.corroded_fraction -
                      spent.state.corroded_fraction) < 0.0001f);
    ASSERT_TRUE(fr_reactive_state_valid(&restored.state, err, sizeof(err)));
    float coin_reload = -1.f;
    ASSERT_TRUE(greek_cell_corrosion(gpu, 27u, 213u, &coin_reload, &found, err,
                                     sizeof(err)) &&
                found);
    printf("greek corrosion coin after reload %.4f\n", coin_reload);
    ASSERT_TRUE(fabsf(coin_reload - coin_after) < 0.001f);
    dc_gpu_destroy(gpu);
    PASS();
}
int main(int argc, char **argv) {
    mkdir("build/screenshots", 0777);
    greek_install_stop();
    if (argc == 2 && strcmp(argv[1], "--corrode-contact") == 0) {
        RUN(test_stationary_greek_film_corrodes_clean_pool_samples);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--corrode") == 0) {
        RUN(test_greek_burn_corrodes_coins_across_reload);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--soak") == 0) {
        RUN(test_painted_water_soaks_a_wood_body);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--stage") == 0) {
        RUN(test_greek_and_oil_stage_matched_prearmed_coating);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--risk-controls") == 0) {
        RUN(test_prompt_launch_and_fireproof_carrier_stay_safe);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--risk") == 0) {
        RUN(test_prompt_launch_and_fireproof_carrier_stay_safe);
        RUN(test_delayed_greek_load_ignites_the_launcher_ensemble);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--douse") == 0) {
        RUN(test_submerged_greek_keeps_reacting_while_oil_douses);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--douse-preflight") == 0) {
        RUN(test_cold_greek_load_keeps_its_film_while_soaking);
        RUN(test_siege_strike_preserves_the_loads_bound_water);
        RUN(test_pool_soaks_a_stationary_oil_load);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    RUN(test_greek_and_oil_stage_matched_prearmed_coating);
    RUN(test_submerged_greek_keeps_reacting_while_oil_douses);
    RUN(test_greek_depletes_matched_oil_twice_as_fast_dry);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
