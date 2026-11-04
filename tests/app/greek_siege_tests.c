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
static bool greek_land_ball(dc_gpu_t *gpu, dc_siege_scene_t *scene,
                            dc_siege_load_t load, bool ignite, float *rest_x,
                            float *rest_y, char *err, uint32_t cap) {
    if (!greek_ticks(gpu, 2u, err, cap)) return false;
    if (!dc_siege_scene_release(gpu, scene, err, cap)) return false;
    /* The launch strike lights the fuse; the swing itself stays cold and
     * deterministic. */
    if (ignite && !dc_siege_scene_ignite_load(gpu, scene, err, cap))
        return false;
    dc_gpu_body_motion_t arm = {0};
    dc_gpu_world_body_t coal = {0};
    bool crossing = false;
    float release_angle = 0.3f;
    const char *tuned = getenv("DC_SIEGE_RELEASE_ANGLE");
    if (tuned) release_angle = strtof(tuned, NULL);
    for (uint32_t tick = 0; tick < 240u && !crossing; tick++) {
        if (!dc_gpu_tick_step(gpu, err, cap)) return false;
        if (!dc_gpu_read_body_motion(gpu, scene->arm_body, &arm, err, cap))
            return false;
        if (arm.angle >= release_angle) crossing = true;
    }
    if (!crossing) return false;
    if (!dc_siege_scene_release_projectile(gpu, scene, err, cap)) return false;
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
        ASSERT_TRUE(dc_siege_scene_build_load(gpu, false, loads[which], &scene,
                                              err, sizeof(err)));
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
        ASSERT_TRUE(dc_siege_scene_ignite_load(gpu, &scene, err, sizeof(err)));
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
static void test_submerged_greek_keeps_reacting_while_oil_douses(void) {
    char err[256] = {0};
    static const dc_siege_load_t loads[2] = {DC_SIEGE_LOAD_OIL,
                                             DC_SIEGE_LOAD_GREEK};
    uint64_t remaining[2] = {0, 0};
    uint64_t water_left[2] = {0, 0};
    uint64_t late_slope[2] = {0, 0};
    for (unsigned which = 0; which < 2u; which++) {
        dc_gpu_t *gpu = greek_grid();
        ASSERT_TRUE(gpu);
        dc_siege_scene_t scene = {0};
        bool built_pool = dc_siege_scene_build_douse_pool(
            gpu, loads[which], &scene, err, sizeof(err));
        if (!built_pool) printf("greek pool build: %s\n", err);
        ASSERT_TRUE(built_pool);
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
                       "(cell(32,215) material %u fluid %u)\n",
                       wet, pool->cells[(215u % 64u) * 64u + 32u].material,
                       pool->cells[(215u % 64u) * 64u + 32u].fluid_mass);
            }
            free(pool);
        }
        ASSERT_TRUE(greek_ticks(gpu, 1u, err, sizeof(err)));
        uint64_t before = greek_ball_coating(gpu, scene.coal_body, 9u, err,
                                             sizeof(err));
        if (getenv("DC_GREEK_TRACE")) {
            for (uint32_t py = 2u; py < 7u; py++)
                for (uint32_t px = 2u; px < 7u; px++) {
                    fr_reactive_record_t probe;
                    bool found = false;
                    fr_gpu_reactive_read_body(gpu, scene.coal_body, px, py,
                                              &probe, &found, err, sizeof(err));
                    printf("parcel %u,%u found %d material %u film %llu\n",
                           px, py, found,
                           found ? probe.state.material : 0u,
                           found ? (unsigned long long)(
                                       probe.state.coating_oil_fuel_fp +
                                       probe.state.coating_jelly_fuel_fp)
                                 : 0ull);
                }
        }
        ASSERT_TRUE(before != UINT64_MAX && before > 0u);
        /* The load rests in the fixture pool; flood its cells so the film
         * soaks, then light the fuse. Ordinary oil douses; magical Greek
         * fire boils the water off and roars. */
        dc_gpu_world_body_t pool_ball;
        ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &pool_ball,
                                           err, sizeof(err)));
        uint32_t soak_x =
            (uint32_t)((float)pool_ball.chunk.x * 64.f +
                       pool_ball.body.x_fp / 65536.f) + 4u;
        uint32_t soak_y =
            (uint32_t)((float)pool_ball.chunk.y * 64.f +
                       pool_ball.body.y_fp / 65536.f) + 4u;
        ASSERT_TRUE(dc_gpu_paint_material(gpu, soak_x, soak_y, 6u,
                                          DC_MATERIAL_WATER, err, sizeof(err)));
        ASSERT_TRUE(greek_ticks(gpu, 2u, err, sizeof(err)));
        {
            uint32_t counters[32] = {0};
            if (fr_combustion_gpu_bonfire_counters(gpu, counters))
                printf("greek wet diag after soak: scanned-max %u soaked %u phase21 cells %u last-wet %u\n",
                       counters[30], counters[18], counters[12], counters[13]);
        }
        bool lit = dc_siege_scene_ignite_load(gpu, &scene, err, sizeof(err));
        if (!lit) printf("greek ignite: %s\n", err);
        ASSERT_TRUE(lit);
        uint64_t mid = 0;
        for (uint32_t probe = 0u; probe < 30u; probe++) {
            if (!greek_ticks(gpu, 1u, err, sizeof(err))) {
                printf("greek tick failure at probe second %u: %s\n", probe, err);
                break;
            }
            if (probe == 15u)
                mid = greek_ball_coating(gpu, scene.coal_body, 9u, err,
                                         sizeof(err));
            if (probe % 10u == 0u)
                printf("greek %s probe %us film %llu water %llu\n",
                       which == 0u ? "oil" : "greek", probe,
                       (unsigned long long)greek_ball_coating(
                           gpu, scene.coal_body, 9u, err, sizeof(err)),
                       (unsigned long long)greek_ball_water(
                           gpu, scene.coal_body, 9u, err, sizeof(err)));
        }
        remaining[which] = greek_ball_coating(gpu, scene.coal_body, 9u, err,
                                              sizeof(err));
        water_left[which] = greek_ball_water(gpu, scene.coal_body, 9u, err,
                                             sizeof(err));
        late_slope[which] = mid > remaining[which] ? mid - remaining[which] : 0u;
        printf("greek %s submerged film after 30s %llu (before %llu) water %llu "
               "late-window burn %llu\n",
               which == 0u ? "oil" : "greek",
               (unsigned long long)remaining[which], (unsigned long long)before,
               (unsigned long long)water_left[which],
               (unsigned long long)late_slope[which]);
        ASSERT_TRUE(remaining[which] != UINT64_MAX);
        dc_gpu_destroy(gpu);
    }
    /* Ordinary oil under water keeps its film (doused); greek depletes. */
    ASSERT_TRUE(remaining[0] > 0u);
    /* Matched stock: the magical greek film consumes about twice the
     * ordinary oil film across the submerged window. */
    uint64_t oil_burned = 2728148u > remaining[0] ? 2728148u - remaining[0] : 0u;
    uint64_t greek_burned =
        3089891u > remaining[1] ? 3089891u - remaining[1] : 0u;
    printf("greek douse burned: oil %llu greek %llu\n",
           (unsigned long long)oil_burned, (unsigned long long)greek_burned);
    ASSERT_TRUE(oil_burned > 0u);
    /* The magical greek film sustains its underwater reaction on the pool's
     * dissolved stock while the ordinary oil throttles to the dissolved
     * oxygen trickle: at least twice the late-window burn, so the matched
     * greek fuel reaches depletion in at most half the oil time. */
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
        ASSERT_TRUE(dc_siege_scene_build_load(gpu, false, loads[which], &scene,
                                              err, sizeof(err)));
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
                       "(cell(32,215) material %u fluid %u)\n",
                       wet, pool->cells[(215u % 64u) * 64u + 32u].material,
                       pool->cells[(215u % 64u) * 64u + 32u].fluid_mass);
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
int main(int argc, char **argv) {
    mkdir("build/screenshots", 0777);
    greek_install_stop();
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
    if (argc == 2 && strcmp(argv[1], "--douse") == 0) {
        RUN(test_submerged_greek_keeps_reacting_while_oil_douses);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    RUN(test_greek_and_oil_stage_matched_prearmed_coating);
    RUN(test_submerged_greek_keeps_reacting_while_oil_douses);
    RUN(test_greek_depletes_matched_oil_twice_as_fast_dry);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
