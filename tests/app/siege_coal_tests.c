#define main rigid_solver_fixture_main
#include "../vulkan/rigid_solver_tests.c"
#undef main

#include <SDL.h>
#include <signal.h>
#include <sys/stat.h>
#include "../../src/app/siege_scene.h"
#include "../../src/app/view_config.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"
#include "../../src/vulkan/reactive_storage_gpu.h"
#include "../../src/vulkan/gas_gpu_state.h"

static volatile sig_atomic_t siege_stopped;
static void siege_stop(int number) {(void)number;siege_stopped = 1;}
static void siege_install_stop(void) {
    signal(SIGINT, siege_stop);
    signal(SIGTERM, siege_stop);
}

static dc_gpu_t *siege_grid(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, SIM_WIDTH, SIM_HEIGHT,
                       "build/shaders/pattern.comp.spv", err, sizeof(err))) {
        fprintf(stderr, "siege grid: %s\n", err);
        return NULL;
    }
#ifdef DC_SIEGE_PRESENTATION
    if (!dc_gpu_set_viewport(gpu, 0, 0, VIEW_WIDTH, VIEW_HEIGHT) ||
        !dc_gpu_set_display_zoom(gpu, 2u)) {
        fprintf(stderr, "siege viewport failed\n");
        dc_gpu_destroy(gpu);
        return NULL;
    }
#endif
    return gpu;
}

static uint32_t siege_tile_slot(const dc_gpu_t *gpu, uint32_t x, uint32_t y) {
    const uint32_t *pages = gpu->page_mapped;
    return pages[(y / 64u) * gpu->page_width + x / 64u];
}

/* Wall loss and visible coverage measured over constructed member parcels. */
typedef struct {
    uint64_t wall_fuel_fp, wall_char_fp, wall_flaming;
    double soot_cover_020, flame_cover_01;
    float wall_top_gas_K;
} siege_sample_t;

static bool siege_wall_state(dc_gpu_t *gpu, siege_sample_t *out, char *err,
                             uint32_t cap) {
    uint64_t fuel = 0, char_mass = 0, flaming = 0;
    fr_reactive_record_t *records = NULL;
    uint32_t count = 0;
    uint64_t next = 0;
    if (!fr_gpu_reactive_export_bodies(gpu, &records, &count, &next, err, cap))
        return false;
    for (uint32_t i = 0; i < count; i++) {
        const fr_reactive_state_t *s = &records[i].state;
        if (s->material != DC_MATERIAL_WOOD) continue;
        uint32_t body = records[i].owner.kind == 2u ? records[i].owner.key[0] : 0u;
        if (body < 0x4000000bu || body > 0x4000000fu) continue;
        fuel += s->fuel_mass_fp;
        char_mass += s->char_mass_fp;
        if (s->flags & FR_REACTIVE_FLAMING) flaming++;
    }
    free(records);
    out->wall_fuel_fp = fuel;
    out->wall_char_fp = char_mass;
    out->wall_flaming = flaming;
    return true;
}

/* Locked coverage thresholds: smoke optical depth 0.2 (renderer opacity 832
 * over reference density 0.0012) or admitted flame emission power 0.1. */
#define SIEGE_SMOKE_AEROSOL_AT_02 (0.2 * 0.0012 / 832.0)
#define SIEGE_FLAME_POWER_AT_01 0.1

static bool siege_viewport_coverage(dc_gpu_t *gpu, siege_sample_t *out, char *err,
                                    uint32_t cap) {
    uint32_t smoke = 0, flame = 0;
    for (uint32_t tile_y = 0; tile_y < 5u; tile_y++)
        for (uint32_t tile_x = 0; tile_x < 8u; tile_x++) {
            uint32_t slot = siege_tile_slot(gpu, tile_x * 64u, tile_y * 64u);
            if (!slot || slot > DC_GPU_CHUNK_SLOTS) return false;
            fr_gas_state_t *states = calloc(4096, sizeof(*states));
            if (!states || !fr_gas_gpu_read_chunk(gpu, slot - 1u, states, 4096,
                                                  err, cap)) {
                free(states);
                return false;
            }
            for (uint32_t i = 0; i < 4096u; i++) {
                uint32_t x = tile_x * 64u + i % 64u;
                uint32_t y = tile_y * 64u + i / 64u;
                if (x >= DC_SIEGE_VIEW_W || y >= DC_SIEGE_VIEW_H) continue;
                double aerosol = (double)states[i].species[FR_GAS_SOOT] +
                                 .05 * (double)states[i].species[FR_GAS_EXHAUST];
                if (aerosol >= SIEGE_SMOKE_AEROSOL_AT_02) smoke++;
            }
            free(states);
            fr_combustion_emission_t *emission = calloc(4096, sizeof(*emission));
            if (!emission ||
                !fr_combustion_gpu_read_emission_chunk(gpu, slot - 1u, emission,
                                                       4096, err, cap)) {
                free(emission);
                return false;
            }
            for (uint32_t i = 0; i < 4096u; i++) {
                uint32_t x = tile_x * 64u + i % 64u;
                uint32_t y = tile_y * 64u + i / 64u;
                if (x >= DC_SIEGE_VIEW_W || y >= DC_SIEGE_VIEW_H) continue;
                if (emission[i].sources && emission[i].power >=
                                               SIEGE_FLAME_POWER_AT_01)
                    flame++;
            }
            free(emission);
        }
    out->soot_cover_020 = (double)smoke;
    out->flame_cover_01 = (double)flame;
    return true;
}

#ifdef DC_SIEGE_PRESENTATION
static bool siege_capture(dc_gpu_t *gpu, const char *path, char *err, uint32_t cap) {
    uint32_t *pixels = malloc(VIEW_WIDTH * VIEW_HEIGHT * 4u);
    if (!pixels) return false;
    bool okay = dc_gpu_readback(gpu, pixels, VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    SDL_Surface *image = okay ? SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, VIEW_WIDTH, VIEW_HEIGHT, 32, VIEW_WIDTH * 4,
        SDL_PIXELFORMAT_ABGR8888) : NULL;
    okay = image && SDL_SaveBMP(image, path) == 0;
    SDL_FreeSurface(image);
    free(pixels);
    return okay;
}
#endif

#ifdef DC_SIEGE_PRESENTATION
#define SIEGE_STEP(gpu, err, cap) dc_gpu_present_chunks_steps(gpu, 1, err, cap)
#else
#define SIEGE_STEP(gpu, err, cap) dc_gpu_tick_step(gpu, err, cap)
#endif

static double siege_frame_mean_ms, siege_frame_worst_ms;
static uint64_t siege_frames;

static bool siege_ticks(dc_gpu_t *gpu, uint32_t seconds, char *err, uint32_t cap) {
    siege_install_stop();
    double total = 0., worst = 0.;
    uint64_t frames = 0;
    uint64_t frequency = SDL_GetPerformanceFrequency();
    for (uint32_t second = 0; second < seconds && !siege_stopped; second++)
        for (uint32_t tick = 0; tick < 60u; tick++) {
            uint64_t began = SDL_GetPerformanceCounter();
            if (!SIEGE_STEP(gpu, err, cap)) return false;
            double ms = (double)(SDL_GetPerformanceCounter() - began) /
                        (double)frequency * 1000.;
            total += ms;
            if (ms > worst) worst = ms;
            frames++;
        }
    if (frames) {
        siege_frame_mean_ms = total / (double)frames;
        siege_frame_worst_ms = worst;
        siege_frames = frames;
    }
    return !siege_stopped;
}

/* The launcher frame is fixed constructed terrain with a kinematic pivot
 * mount; launcher soundness means the mount never moves and the pivot holds. */
static bool siege_launcher_sound(dc_gpu_t *gpu, char *err, uint32_t cap) {
    dc_gpu_body_motion_t mount;
    if (!dc_gpu_read_body_motion(gpu, 0x70000001u, &mount, err, cap)) return false;
    dc_gpu_world_body_t mount_body;
    if (!dc_gpu_read_world_body(gpu, 0x70000001u, &mount_body, err, cap))
        return false;
    float x = (float)mount_body.chunk.x * 64.f + mount_body.body.x_fp / 65536.f;
    float y = (float)mount_body.chunk.y * 64.f + mount_body.body.y_fp / 65536.f;
    printf("siege mount %.1f %.1f kinematic %u\n", x, y, mount.flags & 1u);
    return (mount.flags & 1u) != 0u && fabsf(x - 102.f) < .01f &&
           fabsf(y - 187.f) < .01f;
}

static void test_locked_siege_scene_matches_the_manifest_fixture(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = siege_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    bool built = dc_siege_scene_build(gpu, false, true, &scene, err, sizeof(err));
    if (!built) printf("siege build: %s\n", err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(scene.wall_cells == DC_SIEGE_WALL_W * DC_SIEGE_WALL_H);
    ASSERT_TRUE(scene.wall_fuel_fp > 0u && scene.wall_water_fp == 0u);
    ASSERT_TRUE(scene.coal_cells > 40u);
    ASSERT_TRUE(scene.arm_body != 0u && scene.coal_body != 0u);
    dc_gpu_world_body_t coal, arm;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.arm_body, &arm, err, sizeof(err)));
    float coal_x = (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f;
    float coal_y = (float)coal.chunk.y * 64.f + coal.body.y_fp / 65536.f;
    float arm_x = (float)arm.chunk.x * 64.f + arm.body.x_fp / 65536.f;
    float arm_y = (float)arm.chunk.y * 64.f + arm.body.y_fp / 65536.f;
    printf("siege coal %.1f %.1f arm %.1f %.1f\n", coal_x, coal_y, arm_x, arm_y);
    ASSERT_TRUE(coal_x > 60.f && coal_x < 90.f && coal_y > 145.f && coal_y < 175.f);
    ASSERT_TRUE(arm_x >= DC_SIEGE_ARM_X - 1.f && arm_x <= DC_SIEGE_ARM_X + 1.f);
    fr_reactive_record_t coal_state;
    bool coal_found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u,
                                          &coal_state, &coal_found, err,
                                          sizeof(err)) && coal_found);
    float coal_K = 0.f;
    ASSERT_TRUE(fr_reactive_state_temperature(&coal_state.state, &coal_K, err,
                                              sizeof(err)));
    fr_thermal_inspection_t wall_cell = {0};
    ASSERT_TRUE(fr_thermal_gpu_inspect(gpu, DC_SIEGE_WALL_X + 2u,
                                       DC_SIEGE_WALL_Y + 40u, &wall_cell,
                                       err, sizeof(err)));
    printf("siege coal thermal %.1fK flags %u wall %.1fK wetness %.3f\n",
           coal_K, coal_state.state.flags, wall_cell.temperature_K,
           wall_cell.wetness);
    ASSERT_TRUE(coal_K > 900.f);
    ASSERT_TRUE((coal_state.state.flags & FR_REACTIVE_FLAMING) != 0u);
    ASSERT_TRUE(wall_cell.wetness < .05f);
    ASSERT_TRUE(siege_launcher_sound(gpu, err, sizeof(err)));
    {
        /* The admitted ball must burn on its own finite parcels. */
        uint32_t slot = siege_tile_slot(gpu, 74u, 161u);
        fr_combustion_emission_t *probe = calloc(4096, sizeof(*probe));
        for (uint32_t tick = 0; tick < 300u; tick++) {
            ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
            if (tick == 1u || tick == 11u || tick == 59u || tick == 179u ||
                tick == 299u) {
                double coal_power = 0.;
                uint32_t coal_sources = 0;
                if (probe && slot && fr_combustion_gpu_read_emission_chunk(
                                         gpu, slot - 1u, probe, 4096, err,
                                         sizeof(err)))
                    for (uint32_t i = 0; i < 4096u; i++)
                        if (probe[i].sources) {
                            coal_sources++;
                            coal_power += probe[i].power;
                        }
                printf("siege coal tick %u emission sources %u power %.4g\n",
                       tick + 1u, coal_sources, coal_power);
            }
        }
        free(probe);
        fr_reactive_record_t after_burn;
        bool after_found = false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u,
                                              &after_burn, &after_found, err,
                                              sizeof(err)) && after_found);
        float after_K = 0.f;
        ASSERT_TRUE(fr_reactive_state_temperature(&after_burn.state, &after_K,
                                                  err, sizeof(err)));
        printf("siege coal 300 ticks: %.1fK flags %u fuel %u\n", after_K,
               after_burn.state.flags, after_burn.state.fuel_mass_fp);
        ASSERT_TRUE(after_burn.state.fuel_mass_fp < coal_state.state.fuel_mass_fp);
    }
    fr_gas_gpu_clock_t clock = {0};
    ASSERT_TRUE(fr_gas_gpu_clock(gpu, &clock));
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_wet_siege_control_saturates_the_wall(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = siege_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    bool built = dc_siege_scene_build(gpu, true, true, &scene, err, sizeof(err));
    if (!built) printf("siege wet build: %s\n", err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(scene.wall_water_fp > scene.wall_fuel_fp * 9u / 20u);
    fr_thermal_inspection_t wall_cell = {0};
    ASSERT_TRUE(fr_thermal_gpu_inspect(gpu, DC_SIEGE_WALL_X + 2u,
                                       DC_SIEGE_WALL_Y + 40u, &wall_cell,
                                       err, sizeof(err)));
    fr_reactive_record_t wet_parcel;
    bool wet_found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, 0x4000000bu, 2u, 4u, &wet_parcel,
                                          &wet_found, err, sizeof(err)) &&
                wet_found);
    printf("siege wet wall wetness thermal %.3f parcel water %u of %u\n",
           wall_cell.wetness, wet_parcel.state.bound_water_fp,
           wet_parcel.state.dry_mass_fp);
    ASSERT_TRUE(wet_parcel.state.bound_water_fp >
                wet_parcel.state.dry_mass_fp * 9u / 20u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_cold_projectile_and_prompt_launch_keep_the_launcher_sound(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = siege_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    bool built = dc_siege_scene_build(gpu, false, false, &scene, err, sizeof(err));
    if (!built) printf("siege cold build: %s\n", err);
    ASSERT_TRUE(built);
    siege_sample_t sample = {0};
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    uint64_t wall_initial = sample.wall_fuel_fp;
    ASSERT_TRUE(siege_ticks(gpu, 2u, err, sizeof(err)));
    ASSERT_TRUE(dc_siege_scene_release(gpu, &scene, err, sizeof(err)));
    dc_gpu_body_motion_t arm;
    uint32_t crossing = 0;
    for (uint32_t tick = 0; tick < 240u && !crossing; tick++) {
        ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, scene.arm_body, &arm, err,
                                            sizeof(err)));
        /* Release just before the vertical crossing: the engine's mild
         * in-flight gravity otherwise lets the ball graze the wall top. */
        if (arm.angle >= -0.3f) crossing = tick + 1u;
    }
    ASSERT_TRUE(crossing);
    ASSERT_TRUE(dc_siege_scene_release_projectile(gpu, &scene, err, sizeof(err)));
    ASSERT_TRUE(siege_ticks(gpu, 3u, err, sizeof(err)));
    ASSERT_TRUE(siege_launcher_sound(gpu, err, sizeof(err)));
    {
        dc_gpu_world_body_t probe = {.chunk = {4, 0},
            .body = {.id = 0x70000010u, .x_fp = 40u * 65536u,
                     .y_fp = 40u * 65536u, .width = 4u, .height = 4u,
                     .active = 1u}};
        dc_gpu_body_shape_t box = {.count = 4, .material = DC_GPU_BODY_STONE,
            .vertices = {{0, 0}, {4 << 16, 0}, {4 << 16, 4 << 16}, {0, 4 << 16}}};
        ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, probe, &box, err, sizeof(err)));
        for (uint32_t tick = 0; tick < 60u; tick++)
            ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
        dc_gpu_world_body_t fallen;
        ASSERT_TRUE(dc_gpu_read_world_body(gpu, 0x70000010u, &fallen, err,
                                           sizeof(err)));
        printf("siege free-fall probe y %.1f after 60 ticks (start 40)\n",
               (float)fallen.chunk.y * 64.f + fallen.body.y_fp / 65536.f);
    }
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    ASSERT_TRUE(sample.wall_fuel_fp == wall_initial);
    fr_reactive_record_t coal;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(gpu, scene.coal_body, 1u, 1u, &coal,
                                          &found, err, sizeof(err)));
    ASSERT_EQ(coal.state.material, DC_MATERIAL_COAL);
    ASSERT_TRUE((coal.state.flags & FR_REACTIVE_FLAMING) == 0u);
    ASSERT_TRUE(coal.state.fuel_mass_fp > 0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_hot_coal_impact_burns_a_restrained_wall_fraction(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = siege_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    bool built = dc_siege_scene_build(gpu, false, true, &scene, err, sizeof(err));
    if (!built) printf("siege hot build: %s\n", err);
    ASSERT_TRUE(built);
    siege_sample_t sample = {0};
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    uint64_t wall_initial = sample.wall_fuel_fp;
    ASSERT_TRUE(siege_ticks(gpu, 2u, err, sizeof(err)));
    ASSERT_TRUE(dc_siege_scene_release(gpu, &scene, err, sizeof(err)));
    dc_gpu_body_motion_t arm;
    dc_gpu_world_body_t coal;
    float release_x = 0.f, release_y = 0.f;
    double release_seconds = 0.;
    bool crossing = false;
    for (uint32_t tick = 0; tick < 240u && !crossing; tick++) {
        ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, scene.arm_body, &arm, err,
                                            sizeof(err)));
        if (getenv("DC_SIEGE_TRACE") && tick < 20u) {
            ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal, err,
                                               sizeof(err)));
            printf("siege swing tick %u arm %.3f ball %.1f %.1f vel %.2f %.2f\n",
                   tick, arm.angle,
                   (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f,
                   (float)coal.chunk.y * 64.f + coal.body.y_fp / 65536.f,
                   coal.body.vx_fp / 65536.f, coal.body.vy_fp / 65536.f);
        }
        /* Release just past the vertical crossing: the pouch then throws the
         * ball flat and slightly downward, so the shot strikes the wall face
         * instead of arcing over its top. DC_SIEGE_RELEASE_ANGLE tunes the
         * gate for aiming diagnostics; the locked shot uses the default. */
        float release_angle = 0.3f;
        const char *tuned = getenv("DC_SIEGE_RELEASE_ANGLE");
        if (tuned) release_angle = strtof(tuned, NULL);
        if (arm.angle >= release_angle) {
            crossing = true;
            ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal, err,
                                               sizeof(err)));
            release_x = (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f;
            release_y = (float)coal.chunk.y * 64.f + coal.body.y_fp / 65536.f;
            fr_gas_gpu_clock_t clock = {0};
            ASSERT_TRUE(fr_gas_gpu_clock(gpu, &clock));
            release_seconds = clock.advanced_seconds - 2.;
        }
    }
    ASSERT_TRUE(crossing);
    float release_speed = coal.body.vx_fp / 65536.f;
    printf("siege release pos %.1f %.1f speed %.2f cells/tick (%.0f c/s) at "
           "%.3f s after latch\n",
           release_x, release_y, release_speed, release_speed * 60.f,
           release_seconds);
    ASSERT_TRUE(release_speed > 3.f);
    ASSERT_TRUE(dc_siege_scene_release_projectile(gpu, &scene, err, sizeof(err)));
    double impact_seconds = -1.;
    float impact_speed = 0.f;
    for (uint32_t tick = 0; tick < 60u && impact_seconds < 0.; tick++) {
        ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal, err,
                                           sizeof(err)));
        float coal_x = (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f;
        if (coal_x + 4.f >= DC_SIEGE_WALL_X) {
            impact_seconds = (double)tick / 60.;
            impact_speed = coal.body.vx_fp / 65536.f;
            for (uint32_t follow = 0u; follow < 6u; follow++) {
                ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal,
                                                   err, sizeof(err)));
                printf("siege impact tick %u ball %.1f %.1f vel %.2f %.2f\n",
                       follow,
                       (float)coal.chunk.x * 64.f + coal.body.x_fp / 65536.f,
                       (float)coal.chunk.y * 64.f + coal.body.y_fp / 65536.f,
                       coal.body.vx_fp / 65536.f, coal.body.vy_fp / 65536.f);
                if (follow < 5u) ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
            }
        }
    }
    ASSERT_TRUE(impact_seconds >= 0.);
    printf("siege impact %.3f s after release at %.2f cells/tick "
           "(locked window 0.2-0.3)\n",
           impact_seconds, impact_speed);
    {
        dc_chunk_t *wall_chunk = calloc(1, sizeof(*wall_chunk));
        wall_chunk->coord = (dc_chunk_coord_t){2, 2};
        ASSERT_TRUE(dc_gpu_download_chunk(gpu, siege_tile_slot(gpu, 154u, 170u) - 1u,
                                          wall_chunk, err, sizeof(err)));
        uint32_t at = (170u % 64u) * 64u + 154u % 64u;
        printf("siege wall cell (154,170) material %u flags %x\n",
               wall_chunk->cells[at].material, wall_chunk->cells[at].flags);
        free(wall_chunk);
    }
    /* First visible second after the shot: viewport footprint bound. */
    ASSERT_TRUE(siege_ticks(gpu, 1u, err, sizeof(err)));
    ASSERT_TRUE(siege_viewport_coverage(gpu, &sample, err, sizeof(err)));
    fr_reactive_record_t diag;
    bool diag_found = false;
    if (fr_gpu_reactive_read_body(gpu, scene.coal_body, 4u, 4u, &diag,
                                  &diag_found, err, sizeof(err)) && diag_found) {
        float diag_K = 0.f;
        fr_reactive_state_temperature(&diag.state, &diag_K, err, sizeof(err));
        printf("siege coal after flight: %.1fK flags %u fuel %u char %u\n",
               diag_K, diag.state.flags, diag.state.fuel_mass_fp,
               diag.state.char_mass_fp);
    }
    fr_combustion_emission_t *diag_emission = calloc(4096, sizeof(*diag_emission));
    uint32_t diag_slot = siege_tile_slot(gpu, DC_SIEGE_WALL_X, DC_SIEGE_WALL_Y + 20u);
    if (diag_emission && diag_slot &&
        fr_combustion_gpu_read_emission_chunk(gpu, diag_slot - 1u, diag_emission,
                                              4096, err, sizeof(err))) {
        double peak = 0.;
        uint32_t sources = 0;
        for (uint32_t i = 0; i < 4096u; i++)
            if (diag_emission[i].sources) {
                sources++;
                if (diag_emission[i].power > peak) peak = diag_emission[i].power;
            }
        printf("siege wall tile emission sources %u peak %.4g\n", sources, peak);
    }
    free(diag_emission);
    double footprint = sample.soot_cover_020 + sample.flame_cover_01;
    printf("siege first-second cells soot %.0f flame %.0f of %u (%.2f%%)\n",
           sample.soot_cover_020, sample.flame_cover_01,
           DC_SIEGE_VIEW_W * DC_SIEGE_VIEW_H,
           100. * footprint / (DC_SIEGE_VIEW_W * DC_SIEGE_VIEW_H));
    ASSERT_TRUE(100. * footprint <=
                (double)(DC_SIEGE_VIEW_W * DC_SIEGE_VIEW_H) *
                    DC_SIEGE_VIEWPORT_COVERAGE_MAX_PCT);
#ifdef DC_SIEGE_PRESENTATION
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(siege_capture(gpu, "build/screenshots/siege_coal_impact.bmp",
                              err, sizeof(err)));
#endif
    /* Sixty simulated seconds of restrained burning. */
    for (uint32_t second = 0; second < 60u; second += 5u) {
        ASSERT_TRUE(siege_ticks(gpu, 5u, err, sizeof(err)));
        ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
        double loss = 100. * (double)(wall_initial - sample.wall_fuel_fp) /
                      (double)wall_initial;
        printf("siege wall sim %us loss %.2f%% flaming %llu char %llu\n",
               second + 5u, loss, (unsigned long long)sample.wall_flaming,
               (unsigned long long)sample.wall_char_fp);
#ifdef DC_SIEGE_PRESENTATION
        if (second + 5u == 30u)
            ASSERT_TRUE(siege_capture(gpu, "build/screenshots/siege_coal_mid.bmp",
                                      err, sizeof(err)));
#endif
    }
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    double loss = 100. * (double)(wall_initial - sample.wall_fuel_fp) /
                  (double)wall_initial;
    dc_gpu_world_body_t rest;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &rest, err,
                                       sizeof(err)));
    float rest_x = (float)rest.chunk.x * 64.f + rest.body.x_fp / 65536.f;
    float rest_y = (float)rest.chunk.y * 64.f + rest.body.y_fp / 65536.f;
    dc_gpu_body_motion_t rest_motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, scene.coal_body, &rest_motion, err,
                                        sizeof(err)));
    printf("siege ball rest motion flags %u mass %.3f\n", rest_motion.flags,
           rest_motion.mass);
    fr_thermal_inspection_t wall_face = {0};
    ASSERT_TRUE(fr_thermal_gpu_inspect(gpu, DC_SIEGE_WALL_X, (uint32_t)rest_y,
                                       &wall_face, err, sizeof(err)));
    printf("siege final wall loss %.2f%% (locked window 10-35%%); ball rests "
           "%.1f %.1f; wall face %.1fK wetness %.3f\n",
           loss, rest_x, rest_y, wall_face.temperature_K, wall_face.wetness);
    printf("siege frames %llu mean %.3f ms worst %.3f ms (informational "
           "complete-frame timing; the strict 30 ms gate belongs to ctlo-jjye)\n",
           (unsigned long long)siege_frames, siege_frame_mean_ms,
           siege_frame_worst_ms);
    ASSERT_TRUE(loss >= 10. && loss <= 35.);
    /* The load-bearing core survives: the lowest wall rows stay unburnt. */
    fr_reactive_record_t core;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(
        gpu, siege_tile_slot(gpu, DC_SIEGE_WALL_X, DC_SIEGE_WALL_Y + 76u) - 1u,
        ((DC_SIEGE_WALL_Y + 76u) % 64u) * 64u + DC_SIEGE_WALL_X % 64u, &core,
        &found, err, sizeof(err)) && found);
    ASSERT_TRUE(core.state.fuel_mass_fp > 0u);
    ASSERT_TRUE((core.state.flags & FR_REACTIVE_FLAMING) == 0u);
    ASSERT_TRUE(siege_launcher_sound(gpu, err, sizeof(err)));
#ifdef DC_SIEGE_PRESENTATION
    ASSERT_TRUE(siege_capture(gpu, "build/screenshots/siege_coal_60s.bmp", err,
                              sizeof(err)));
#endif
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_very_wet_wall_resists_sustained_ignition(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = siege_grid();
    ASSERT_TRUE(gpu);
    dc_siege_scene_t scene = {0};
    bool built = dc_siege_scene_build(gpu, true, true, &scene, err, sizeof(err));
    if (!built) printf("siege wet shot build: %s\n", err);
    ASSERT_TRUE(built);
    siege_sample_t sample = {0};
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    uint64_t wall_initial = sample.wall_fuel_fp;
    uint64_t water_initial = scene.wall_water_fp;
    ASSERT_TRUE(siege_ticks(gpu, 2u, err, sizeof(err)));
    ASSERT_TRUE(dc_siege_scene_release(gpu, &scene, err, sizeof(err)));
    dc_gpu_body_motion_t arm;
    bool crossing = false;
    for (uint32_t tick = 0; tick < 240u && !crossing; tick++) {
        ASSERT_TRUE(SIEGE_STEP(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, scene.arm_body, &arm, err,
                                            sizeof(err)));
        /* Release just before the vertical crossing: the engine's mild
         * in-flight gravity otherwise lets the ball graze the wall top. */
        if (arm.angle >= -0.3f) crossing = true;
    }
    ASSERT_TRUE(crossing);
    ASSERT_TRUE(dc_siege_scene_release_projectile(gpu, &scene, err, sizeof(err)));
    for (uint32_t second = 0; second < 60u; second += 10u) {
        ASSERT_TRUE(siege_ticks(gpu, 10u, err, sizeof(err)));
        ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
        double loss = 100. * (double)(wall_initial - sample.wall_fuel_fp) /
                      (double)wall_initial;
        uint32_t sustained = 0;
        for (uint32_t check = 0; check < 3u; check++) {
            if (siege_wall_state(gpu, &sample, err, sizeof(err)) &&
                sample.wall_flaming > 6u)
                sustained++;
        }
        printf("siege wet wall sim %us loss %.2f%% flaming %llu sustained %u\n",
               second + 10u, loss, (unsigned long long)sample.wall_flaming,
               sustained);
        ASSERT_TRUE(loss < 2.);
    }
    ASSERT_TRUE(siege_wall_state(gpu, &sample, err, sizeof(err)));
    ASSERT_TRUE(sample.wall_flaming <= 6u);
    ASSERT_TRUE(sample.wall_fuel_fp >= wall_initial * 19u / 20u);
    (void)water_initial;
#ifdef DC_SIEGE_PRESENTATION
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(siege_capture(gpu, "build/screenshots/siege_coal_wet_wall.bmp",
                              err, sizeof(err)));
#endif
    dc_gpu_destroy(gpu);
    PASS();
}

#ifndef DC_SIEGE_DISPLAY_INCLUDE
int main(int argc, char **argv) {
    mkdir("build/screenshots", 0777);
    if (argc == 2 && strcmp(argv[1], "--build-only") == 0) {
        RUN(test_locked_siege_scene_matches_the_manifest_fixture);
        RUN(test_wet_siege_control_saturates_the_wall);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--shot") == 0) {
        RUN(test_hot_coal_impact_burns_a_restrained_wall_fraction);
        printf("%d passed, %d failed\n", g_pass, g_fail);
        return g_fail ? 1 : 0;
    }
    RUN(test_locked_siege_scene_matches_the_manifest_fixture);
    RUN(test_wet_siege_control_saturates_the_wall);
    RUN(test_cold_projectile_and_prompt_launch_keep_the_launcher_sound);
    RUN(test_hot_coal_impact_burns_a_restrained_wall_fraction);
    RUN(test_very_wet_wall_resists_sustained_ignition);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
#endif
