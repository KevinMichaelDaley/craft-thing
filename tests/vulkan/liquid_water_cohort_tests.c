#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/liquid_seam.h"
#include "ferrum/fire/bulk_phase_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/renderer_internal.h"

enum { CENTER, EAST, SOUTH, TILE_COUNT, EXCLUDED = TILE_COUNT, FIXTURE_TILES };
static const float step_seconds = 1.f / 30.f;
static int g_pass, g_fail;
static bool g_lose_ack, g_drop_submit;
static bool g_forbid_queue;
static char g_err[256];

bool __real_fr_renderer_submit(dc_gpu_t *gpu);
VkResult __real_vkQueueWaitIdle(VkQueue queue);

VkResult __wrap_vkQueueWaitIdle(VkQueue queue) {
    if (g_forbid_queue) return VK_ERROR_DEVICE_LOST;
    return __real_vkQueueWaitIdle(queue);
}

bool __wrap_fr_renderer_submit(dc_gpu_t *gpu) {
    if (g_drop_submit) {
        g_drop_submit = false;
        vkEndCommandBuffer(gpu->command);
        return false;
    }
    const bool lose = g_lose_ack;
    g_lose_ack = false;
    return __real_fr_renderer_submit(gpu) && !lose;
}

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #expr, g_err); ++g_fail; goto cleanup; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

typedef struct {
    dc_chunk_t chunk;
    dc_chunk_colors_t colors;
    float heat[DC_CHUNK_CELLS];
    uint32_t guide[DC_CHUNK_CELLS];
} water_tile_t;

static void open_corridor(water_tile_t *tile, bool vertical, bool donor) {
    const uint32_t lane = 56;
    for (uint32_t normal = donor ? 48u : 0u; normal < DC_CHUNK_SIDE; ++normal) {
        const uint32_t cell = vertical ? normal * DC_CHUNK_SIDE + lane : lane * DC_CHUNK_SIDE + normal;
        tile->chunk.cells[cell].material = DC_MATERIAL_AIR;
        if (vertical) tile->chunk.face_velocity[cell].y = 6.25f;
        else tile->chunk.face_velocity[cell].x = 7.5f;
        if (!donor || normal < 53u) continue;
        tile->chunk.cells[cell].fluid_mass = DC_FLUID_FULL / 2u;
        tile->colors.water[cell][vertical ? 2 : 0] = .85f;
        tile->colors.water[cell][3] = 1.f;
        tile->heat[cell] = vertical ? -1.5f : 2.f;
    }
}

static void seed(water_tile_t tiles[TILE_COUNT], const dc_liquid_seam_endpoint_t owners[TILE_COUNT]) {
    for (uint32_t owner = 0; owner < TILE_COUNT; ++owner) {
        tiles[owner].chunk.coord = owners[owner].coord;
        for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell)
            tiles[owner].chunk.cells[cell].material = DC_MATERIAL_STONE;
    }
    open_corridor(&tiles[CENTER], false, true);
    open_corridor(&tiles[CENTER], true, true);
    open_corridor(&tiles[EAST], false, false);
    open_corridor(&tiles[SOUTH], true, false);
    dc_chunk_t *center = &tiles[CENTER].chunk;
    center->marker_count = 1;
    center->markers[0] = (dc_marker_t){62u * DC_FLUID_FULL, 56u * DC_FLUID_FULL + DC_FLUID_FULL / 2u,
                                      1001u, DC_MARKER_INSIDE};
}

static bool upload(dc_liquid_seam_endpoint_t tile, const water_tile_t *state) {
    return dc_gpu_upload_chunk(tile.gpu, tile.slot, &state->chunk, g_err, sizeof(g_err)) &&
        dc_gpu_set_page(tile.gpu, tile.tile_x, tile.tile_y, tile.slot, g_err, sizeof(g_err)) &&
        fr_renderer_upload_chunk_colors(tile.gpu, tile.slot, &state->colors, g_err, sizeof(g_err)) &&
        fr_thermal_gpu_import_water_chunk(tile.gpu, tile.slot, &state->chunk, state->heat, g_err, sizeof(g_err));
}

static bool download(dc_liquid_seam_endpoint_t tile, water_tile_t *state) {
    if (!dc_gpu_download_chunk(tile.gpu, tile.slot, &state->chunk, g_err, sizeof(g_err)) ||
        !fr_renderer_download_chunk_colors(tile.gpu, tile.slot, &state->colors, g_err, sizeof(g_err)) ||
        !fr_thermal_gpu_read_water_chunk(tile.gpu, tile.slot, state->heat, DC_CHUNK_CELLS, g_err, sizeof(g_err)))
        return false;
    memcpy(state->guide, (const uint32_t *)tile.gpu->marker_grid_mapped + tile.slot * DC_CHUNK_CELLS,
           sizeof(state->guide));
    return true;
}

static void set_apertures(dc_liquid_seam_endpoint_t center, uint32_t x_coverage, uint32_t y_coverage) {
    dc_gpu_t *gpu = center.gpu;
    uint32_t *occupancy = gpu->occupancy_mapped;
    const uint32_t cells = gpu->width * gpu->height;
    const uint32_t base = center.tile_y * DC_CHUNK_SIDE * gpu->width + center.tile_x * DC_CHUNK_SIDE;
    occupancy[3u * cells + base + 56u * gpu->width + 63u] = x_coverage;
    occupancy[4u * cells + base + 63u * gpu->width + 56u] = y_coverage;
    gpu->rigid_occupancy_present = true;
}

static void seed_apertures(dc_liquid_seam_endpoint_t center) {
    set_apertures(center, 3u * DC_FLUID_FULL / 4u, DC_FLUID_FULL / 4u);
}

static int compare_markers(const void *a, const void *b) {
    const dc_marker_t *left = a, *right = b;
#define COMPARE(field) if (left->field != right->field) return left->field < right->field ? -1 : 1
    COMPARE(id);
    COMPARE(x_fp);
    COMPARE(y_fp);
    COMPARE(kind);
#undef COMPARE
    return 0;
}

static bool same_water(water_tile_t *a, water_tile_t *b) {
    if (a->chunk.marker_count != b->chunk.marker_count ||
        memcmp(a->chunk.cells, b->chunk.cells, sizeof(a->chunk.cells)) ||
        memcmp(a->guide, b->guide, sizeof(a->guide))) return false;
    const uint32_t markers = a->chunk.marker_count;
    qsort(a->chunk.markers, markers, sizeof(dc_marker_t), compare_markers);
    qsort(b->chunk.markers, markers, sizeof(dc_marker_t), compare_markers);
    if (memcmp(a->chunk.markers, b->chunk.markers, markers * sizeof(dc_marker_t))) return false;
    for (uint32_t cell = 0; cell < DC_CHUNK_CELLS; ++cell) {
        const dc_face_velocity_t va = a->chunk.face_velocity[cell], vb = b->chunk.face_velocity[cell];
        if (!(fabsf(va.x - vb.x) <= 1e-5f && fabsf(va.y - vb.y) <= 1e-5f &&
              fabsf(a->heat[cell] - b->heat[cell]) <= 1e-5f)) return false;
        for (uint32_t color = 0; color < 4u; ++color)
            if (!(fabsf(a->colors.water[cell][color] - b->colors.water[cell][color]) <= 1e-5f)) return false;
    }
    return true;
}

static uint32_t stock(const water_tile_t *tile) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i) sum += tile->chunk.cells[i].fluid_mass;
    return sum;
}

static void test_connected_tiles_match_monolithic_water_and_share_one_receipt(void) {
    const char shader[] = "build/shaders/pattern.comp.spv";
    const dc_chunk_coord_t origin = {-7, -3};
    dc_gpu_t device = {0}, *resident = NULL, *offscreen = NULL, *mono = NULL;
    water_tile_t *initial = calloc(FIXTURE_TILES, sizeof(*initial));
    water_tile_t *before = calloc(FIXTURE_TILES, sizeof(*before));
    water_tile_t *published = calloc(TILE_COUNT, sizeof(*published));
    water_tile_t *observed = calloc(1, sizeof(*observed));
    dc_liquid_seam_endpoint_t tiles[FIXTURE_TILES] = {
        [CENTER] = {.coord = origin, .tile_x = 1, .tile_y = 1, .slot = 2},
        [EAST] = {.coord = {origin.x + 1, origin.y}, .tile_x = 1, .slot = 3},
        [SOUTH] = {.coord = {origin.x, origin.y + 1}, .tile_y = 1, .slot = 1},
        [EXCLUDED] = {.coord = {origin.x + 1, origin.y + 1}, .tile_x = 1, .tile_y = 1, .slot = 0}
    };
    ASSERT_TRUE(initial && before && published && observed);
    seed(initial, tiles);
    initial[EXCLUDED].chunk.coord = tiles[EXCLUDED].coord;
    open_corridor(&initial[EXCLUDED], false, true);
    ASSERT_TRUE(dc_gpu_initialize_device(&device, 4u * DC_CHUNK_CELLS * sizeof(dc_cell_t),
                                        0, 0, 0, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&resident, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&offscreen, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(resident, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(offscreen, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(resident, (dc_chunk_coord_t){origin.x - 1, origin.y - 1},
                                      g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(offscreen, origin, g_err, sizeof(g_err)));
    tiles[CENTER].gpu = resident;
    tiles[EAST].gpu = tiles[SOUTH].gpu = tiles[EXCLUDED].gpu = offscreen;
    offscreen->marker_ping = 1u;
    for (uint32_t tile = 0; tile < FIXTURE_TILES; ++tile) {
        ASSERT_TRUE(upload(tiles[tile], &initial[tile]));
        ASSERT_TRUE(download(tiles[tile], &before[tile]));
    }
    seed_apertures(tiles[CENTER]);

    dc_liquid_seam_sample_t samples[DC_CHUNK_SIDE];
    ASSERT_TRUE(dc_gpu_project_liquid_seam(tiles[CENTER], tiles[EAST], DC_LIQUID_SEAM_X,
        step_seconds, NULL, samples, g_err, sizeof(g_err)));
    ASSERT_EQ(samples[56].aperture_fp, DC_FLUID_FULL / 4u);

    dc_liquid_seam_endpoint_t reordered[] = {tiles[CENTER], tiles[SOUTH], tiles[EAST]};
    reordered[1].slot = tiles[EAST].slot;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_REJECTED);
    reordered[1] = reordered[0];
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_REJECTED);
    reordered[1] = tiles[SOUTH];
    float *bad_color = (float *)offscreen->water_color_mapped +
        (tiles[SOUTH].slot * DC_CHUNK_CELLS + DC_CHUNK_CELLS - 1u) * 4u;
    const float original_color = *bad_color;
    *bad_color = NAN;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_REJECTED);
    ASSERT_TRUE(isnan(*bad_color));
    *bad_color = original_color;
    ASSERT_TRUE(!dc_gpu_liquid_seam_pending(resident) && !dc_gpu_liquid_seam_pending(offscreen));
    for (uint32_t tile = 0; tile < FIXTURE_TILES; ++tile) {
        ASSERT_TRUE(download(tiles[tile], observed));
        ASSERT_TRUE(same_water(observed, &before[tile]));
    }

    g_lose_ack = true;
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_TRUE(dc_gpu_liquid_seam_pending(resident) && dc_gpu_liquid_seam_pending(offscreen));
    ASSERT_EQ(dc_gpu_execute_water_cohort(reordered, TILE_COUNT, step_seconds, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_EQ(dc_gpu_resolve_liquid_seam(offscreen, resident, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_TRUE(!dc_gpu_liquid_seam_pending(resident) && !dc_gpu_liquid_seam_pending(offscreen));
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile)
        ASSERT_TRUE(download(tiles[tile], &published[tile]));
    ASSERT_TRUE(download(tiles[EXCLUDED], observed));
    ASSERT_TRUE(same_water(observed, &before[EXCLUDED]));
    ASSERT_TRUE(stock(&published[EAST]) > 0 && stock(&published[SOUTH]) > 0);
    ASSERT_EQ(stock(&published[CENTER]) + stock(&published[EAST]) + stock(&published[SOUTH]),
              stock(&initial[CENTER]));
    dc_gpu_destroy(resident);
    dc_gpu_destroy(offscreen);
    resident = offscreen = NULL;

    ASSERT_TRUE(dc_gpu_create_shared(&mono, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(mono, origin, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(mono, g_err, sizeof(g_err)));
    const dc_liquid_seam_endpoint_t monolithic[] = {
        {.gpu = mono, .coord = tiles[CENTER].coord, .slot = 0},
        {.gpu = mono, .coord = tiles[EAST].coord, .tile_x = 1, .slot = 1},
        {.gpu = mono, .coord = tiles[SOUTH].coord, .tile_y = 1, .slot = 2}
    };
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) ASSERT_TRUE(upload(monolithic[tile], &initial[tile]));
    seed_apertures(monolithic[CENTER]);
    mono->timed_fluid = true;
    mono->fluid_step_scale = step_seconds * 60.f;
    ASSERT_TRUE(dc_gpu_fluid_step(mono, g_err, sizeof(g_err)));
    for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) {
        ASSERT_TRUE(download(monolithic[tile], observed));
        ASSERT_TRUE(same_water(&published[tile], observed));
    }
    ASSERT_INT_EQ(dc_gpu_validation_errors(&device), 0);
    PASS();

cleanup:
    g_lose_ack = false;
    dc_gpu_destroy(mono);
    dc_gpu_destroy(offscreen);
    dc_gpu_destroy(resident);
    dc_gpu_release_device(&device);
    free(observed);
    free(published);
    free(before);
    free(initial);
}

static bool native_water_tick(dc_gpu_t *gpu, float elapsed) {
    if (!fr_renderer_begin(gpu)) return false;
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    if (gpu->fluid_interval == 1u) {
        gpu->fluid_step_scale = elapsed;
        dc_gpu_record_fluid(gpu);
    } else {
        gpu->fluid_step_scale = (float)gpu->fluid_interval;
        gpu->fluid_phase_budget += elapsed * (6.f / (float)gpu->fluid_interval);
        while (gpu->fluid_phase_budget >= 1.f) {
            dc_gpu_record_fluid_phase(gpu, gpu->fluid_phase);
            gpu->fluid_phase = (gpu->fluid_phase + 1u) % 6u;
            gpu->fluid_phase_budget -= 1.f;
        }
    }
    fr_renderer_barrier(gpu->command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(gpu);
}

typedef struct {
    water_tile_t tiles[TILE_COUNT];
    uint32_t phase, tick;
    float budget;
} water_tick_t;

static void test_coupled_water_matches_every_native_tick_and_recovers_once(void) {
    static const float elapsed[] = {.25f, .75f, 1.f, 2.f, 1.5f, .5f, 6.f};
    static const uint32_t intervals[] = {1u, 2u, 3u, 6u};
    enum { TICKS = sizeof(elapsed) / sizeof(elapsed[0]), INTERVALS = sizeof(intervals) / sizeof(intervals[0]) };
    const char shader[] = "build/shaders/pattern.comp.spv";
    const dc_chunk_coord_t origin = {-9, -5};
    dc_gpu_t device = {0}, *resident = NULL, *offscreen = NULL, *mono = NULL;
    water_tile_t *initial = calloc(TILE_COUNT, sizeof(*initial)), *observed = calloc(1, sizeof(*observed));
    water_tick_t (*split)[TICKS] = calloc(INTERVALS, sizeof(*split));
    dc_liquid_seam_endpoint_t tiles[] = {
        {.coord = origin, .slot = 1, .tile_y = 1},
        {.coord = {origin.x + 1, origin.y}, .tile_x = 1, .slot = 3},
        {.coord = {origin.x, origin.y + 1}, .tile_y = 1, .slot = 1}
    };
    ASSERT_TRUE(initial && observed && split);
    seed(initial, tiles);
    ASSERT_TRUE(dc_gpu_initialize_device(&device, 4u * DC_CHUNK_CELLS * sizeof(dc_cell_t),
                                        0, 0, 0, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&resident, &device, 64, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_create_shared(&offscreen, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(resident, (dc_chunk_coord_t){origin.x, origin.y - 1}, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(offscreen, origin, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(resident, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(offscreen, g_err, sizeof(g_err)));
    tiles[CENTER].gpu = resident;
    tiles[EAST].gpu = tiles[SOUTH].gpu = offscreen;
    for (uint32_t i = 0; i < INTERVALS; ++i) {
        ASSERT_TRUE(dc_gpu_set_fluid_interval(resident, intervals[i]) &&
                    dc_gpu_set_fluid_interval(offscreen, intervals[i]));
        resident->fluid_tick = offscreen->fluid_tick = 0;
        resident->marker_ping = 0;
        offscreen->marker_ping = 1;
        resident->marker_correction = offscreen->marker_correction = i != 2u;
        memset(resident->marker_grid_mapped, 0, dc_gpu_slot_capacity(resident) * sizeof(observed->guide));
        memset(offscreen->marker_grid_mapped, 0, dc_gpu_slot_capacity(offscreen) * sizeof(observed->guide));
        for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) ASSERT_TRUE(upload(tiles[tile], &initial[tile]));
        seed_apertures(tiles[CENTER]);
        for (uint32_t step = 0; step < TICKS; ++step) {
            if (step == 3u) set_apertures(tiles[CENTER], DC_FLUID_FULL, 0u);
            const bool lose_ack = step == 2u;
            if (lose_ack) {
                const uint32_t phase = resident->fluid_phase, tick = resident->fluid_tick;
                const float budget = resident->fluid_phase_budget;
                if (phase != 0u) {
                    ASSERT_TRUE(!dc_gpu_tick_step(resident, g_err, sizeof(g_err)));
                    ASSERT_TRUE(!dc_gpu_set_fluid_interval(offscreen, intervals[i]));
                    ASSERT_TRUE(!dc_gpu_set_page(offscreen, 1u, 0u, UINT32_MAX, g_err, sizeof(g_err)));
                    ASSERT_TRUE(!dc_gpu_upload_chunk(offscreen, tiles[EAST].slot, &initial[EAST].chunk,
                                                     g_err, sizeof(g_err)));
                    const dc_liquid_seam_endpoint_t reordered[] = {tiles[CENTER], tiles[SOUTH], tiles[EAST]};
                    ASSERT_EQ(dc_gpu_step_water_cohort(reordered, TILE_COUNT, elapsed[step] / 60.f, g_err, sizeof(g_err)),
                              DC_LIQUID_SEAM_REJECTED);
                    float *color = (float *)offscreen->water_color_mapped +
                        ((tiles[SOUTH].slot + 1u) * DC_CHUNK_CELLS - 1u) * 4u;
                    const float saved = *color;
                    *color = NAN;
                    ASSERT_EQ(dc_gpu_step_water_cohort(tiles, TILE_COUNT, elapsed[step] / 60.f, g_err, sizeof(g_err)),
                              DC_LIQUID_SEAM_REJECTED);
                    ASSERT_EQ(resident->fluid_phase, phase);
                    ASSERT_EQ(offscreen->fluid_phase, phase);
                    ASSERT_EQ(resident->fluid_phase_budget, budget);
                    ASSERT_TRUE(isnan(*color));
                    *color = saved;
                }
                g_lose_ack = true;
                ASSERT_EQ(dc_gpu_step_water_cohort(tiles, TILE_COUNT, elapsed[step] / 60.f, g_err, sizeof(g_err)),
                          DC_LIQUID_SEAM_UNCONFIRMED);
                ASSERT_EQ(resident->fluid_phase, phase);
                ASSERT_EQ(resident->fluid_tick, tick);
                ASSERT_EQ(resident->fluid_phase_budget, budget);
                ASSERT_EQ(offscreen->fluid_phase, phase);
                ASSERT_EQ(offscreen->fluid_tick, tick);
                ASSERT_EQ(offscreen->fluid_phase_budget, budget);
                ASSERT_EQ(dc_gpu_resolve_liquid_seam(offscreen, resident, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
            } else {
                ASSERT_EQ(dc_gpu_step_water_cohort(tiles, TILE_COUNT, elapsed[step] / 60.f, g_err, sizeof(g_err)),
                          DC_LIQUID_SEAM_COMMITTED);
            }
            water_tick_t *result = &split[i][step];
            result->phase = resident->fluid_phase;
            result->tick = resident->fluid_tick;
            result->budget = resident->fluid_phase_budget;
            ASSERT_EQ(offscreen->fluid_phase, result->phase);
            ASSERT_EQ(offscreen->fluid_tick, result->tick);
            ASSERT_EQ(offscreen->fluid_phase_budget, result->budget);
            for (uint32_t tile = 0; tile < TILE_COUNT; ++tile)
                ASSERT_TRUE(download(tiles[tile], &result->tiles[tile]));
        }
        ASSERT_EQ(resident->fluid_phase, 0u);
        ASSERT_EQ(resident->fluid_tick, intervals[i] == 1u ? TICKS : 12u / intervals[i]);
    }
    const uint32_t final_tick = resident->fluid_tick;
    g_drop_submit = true;
    ASSERT_EQ(dc_gpu_step_water_cohort(tiles, TILE_COUNT, 1.f / 60.f, g_err, sizeof(g_err)),
              DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_EQ(dc_gpu_resolve_liquid_seam(resident, offscreen, g_err, sizeof(g_err)), DC_LIQUID_SEAM_UNCONFIRMED);
    ASSERT_EQ(resident->fluid_phase, 0u);
    ASSERT_EQ(offscreen->fluid_phase, 0u);
    ASSERT_EQ(resident->fluid_phase_budget, 0.f);
    ASSERT_EQ(offscreen->fluid_phase_budget, 0.f);
    ASSERT_EQ(resident->fluid_tick, final_tick);
    ASSERT_EQ(offscreen->fluid_tick, final_tick);
    ASSERT_TRUE(!dc_gpu_set_page(resident, 0u, 1u, UINT32_MAX, g_err, sizeof(g_err)));
    ASSERT_TRUE(!dc_gpu_upload_chunk(resident, tiles[CENTER].slot, &initial[CENTER].chunk,
                                     g_err, sizeof(g_err)));
    dc_gpu_destroy(resident);
    dc_gpu_destroy(offscreen);
    resident = offscreen = NULL;

    ASSERT_TRUE(dc_gpu_create_shared(&mono, &device, 128, 128, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(mono, origin, g_err, sizeof(g_err)));
    ASSERT_TRUE(fr_thermal_gpu_enable(mono, g_err, sizeof(g_err)));
    const dc_liquid_seam_endpoint_t monolithic[] = {
        {.gpu = mono, .coord = tiles[CENTER].coord, .slot = 0},
        {.gpu = mono, .coord = tiles[EAST].coord, .tile_x = 1, .slot = 1},
        {.gpu = mono, .coord = tiles[SOUTH].coord, .tile_y = 1, .slot = 2}
    };
    for (uint32_t i = 0; i < INTERVALS; ++i) {
        ASSERT_TRUE(dc_gpu_set_fluid_interval(mono, intervals[i]));
        mono->timed_fluid = true;
        mono->fluid_tick = 0;
        mono->marker_ping = 0;
        mono->marker_correction = i != 2u;
        memset(mono->marker_grid_mapped, 0, dc_gpu_slot_capacity(mono) * sizeof(observed->guide));
        for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) ASSERT_TRUE(upload(monolithic[tile], &initial[tile]));
        seed_apertures(monolithic[CENTER]);
        for (uint32_t step = 0; step < TICKS; ++step) {
            if (step == 3u) set_apertures(monolithic[CENTER], DC_FLUID_FULL, 0u);
            const float seconds = elapsed[step] / 60.f;
            ASSERT_TRUE(native_water_tick(mono, seconds * 60.f));
            water_tick_t *result = &split[i][step];
            ASSERT_EQ(result->phase, mono->fluid_phase);
            ASSERT_EQ(result->tick, mono->fluid_tick);
            ASSERT_EQ(result->budget, mono->fluid_phase_budget);
            for (uint32_t tile = 0; tile < TILE_COUNT; ++tile) {
                ASSERT_TRUE(download(monolithic[tile], observed));
                ASSERT_TRUE(same_water(&result->tiles[tile], observed));
            }
        }
    }
    ASSERT_EQ(dc_gpu_validation_errors(&device), 0u);
    PASS();

cleanup:
    g_lose_ack = false;
    g_drop_submit = false;
    dc_gpu_destroy(mono);
    dc_gpu_destroy(offscreen);
    dc_gpu_destroy(resident);
    dc_gpu_release_device(&device);
    free(split);
    free(observed);
    free(initial);
}

static void test_fractional_tick_without_a_phase_keeps_the_queue_idle(void) {
    uint32_t pages[] = {1u, 1u};
    dc_gpu_t first = {.width = 64, .height = 64, .page_width = 1, .page_height = 1,
                      .page_mapped = &pages[0], .fluid_interval = 6u};
    dc_gpu_t second = {.width = 64, .height = 64, .page_width = 1, .page_height = 1,
                       .page_mapped = &pages[1], .fluid_interval = 6u, .body_origin = {1, 0}};
    void *identity = malloc(1u);
    ASSERT_TRUE(identity);
    first.device_lifetime = second.device_lifetime = identity;
    const dc_liquid_seam_endpoint_t tiles[] = {{.gpu = &first}, {.gpu = &second, .coord = {1, 0}}};
    g_forbid_queue = true;
    ASSERT_EQ(dc_gpu_step_water_cohort(tiles, 2u, 1.f / 240.f, g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
    ASSERT_EQ(first.fluid_phase_budget, .25f);
    ASSERT_EQ(second.fluid_phase_budget, .25f);
    ASSERT_EQ(first.fluid_tick, 0u);
    ASSERT_EQ(second.fluid_tick, 0u);
    PASS();
cleanup:
    g_forbid_queue = false;
    free(identity);
}

static void check_bulk_crossing(bool vertical) {
    const char shader[] = "build/shaders/pattern.comp.spv";
    dc_gpu_t *owners[2] = {0}, *mono = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)), *native = calloc(1, sizeof(*native));
    fr_bulk_phase_chunk_t *phase = calloc(1, sizeof(*phase));
    fr_bulk_phase_chunk_t *expected = calloc(1, sizeof(*expected));
    dc_liquid_seam_endpoint_t endpoints[2] = {0};
    ASSERT_TRUE(chunk && native && phase && expected);
    ASSERT_TRUE(dc_gpu_create(&owners[0], 64, 64, shader, g_err, sizeof(g_err)) &&
                dc_gpu_create_shared(&owners[1], owners[0], 64, 64, shader, g_err, sizeof(g_err)) &&
                dc_gpu_create(&mono, vertical ? 64 : 128, vertical ? 128 : 64, shader, g_err, sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(mono, 6u));
    for (unsigned i = 0; i < 2; ++i) {
        chunk->coord = (dc_chunk_coord_t){vertical ? 0 : i, vertical ? i : 0};
        if (vertical) {
            for (unsigned j = 0; j < DC_CHUNK_CELLS; ++j) chunk->cells[j] = (dc_cell_t){.material = DC_MATERIAL_STONE};
            unsigned row = i ? 0u : DC_CHUNK_SIDE - 1u;
            for (unsigned x = 30; x <= 34; x += 4)
                chunk->cells[row * DC_CHUNK_SIDE + x] = (dc_cell_t){.fluid_mass = i ? DC_FLUID_FULL : 0u};
        }
        endpoints[i] = (dc_liquid_seam_endpoint_t){.gpu = owners[i], .coord = chunk->coord};
        ASSERT_TRUE(dc_gpu_set_body_origin(owners[i], chunk->coord, g_err, sizeof(g_err)) &&
                    dc_gpu_set_fluid_interval(owners[i], 6u) &&
                    dc_gpu_upload_chunk(owners[i], 0, chunk, g_err, sizeof(g_err)) &&
                    dc_gpu_set_page(owners[i], 0, 0, 0, g_err, sizeof(g_err)) &&
                    dc_gpu_upload_chunk(mono, i, chunk, g_err, sizeof(g_err)) &&
                    dc_gpu_set_page(mono, chunk->coord.x, chunk->coord.y, i, g_err, sizeof(g_err)));
        if (i) continue;
        uint32_t at = vertical ? 63u * DC_CHUNK_SIDE + 30u : 30u * DC_CHUNK_SIDE + 63u;
        phase->oil_fp[at] = DC_FLUID_FULL;
        phase->oil_momentum_x[at] = 10.f;
        phase->oil_excess[at] = 3.f;
        at += vertical ? 4u : 4u * DC_CHUNK_SIDE;
        phase->slime_fp[at] = DC_FLUID_FULL;
        phase->slime_momentum_x[at] = 100.f;
        phase->slime_excess[at] = 6.f;
        ASSERT_TRUE(fr_bulk_phase_gpu_import_chunk(owners[0], 0, chunk, phase, g_err, sizeof(g_err)) &&
                    fr_bulk_phase_gpu_import_chunk(mono, 0, chunk, phase, g_err, sizeof(g_err)));
    }
    const float elapsed[] = {1.f/240.f, 1.f/60.f, 1.f/60.f};
    for (unsigned tick = 0; tick < sizeof(elapsed)/sizeof(*elapsed); ++tick) {
        ASSERT_TRUE(dc_gpu_set_tick_seconds(mono, elapsed[tick]) && dc_gpu_tick_step(mono, g_err, sizeof(g_err)));
        ASSERT_EQ(dc_gpu_tick_water_cohort(endpoints, 2, elapsed[tick], g_err, sizeof(g_err)), DC_LIQUID_SEAM_COMMITTED);
        for (unsigned i = 0; i < 2; ++i) {
            ASSERT_TRUE(fr_bulk_phase_gpu_export_chunk(mono, i, expected, g_err, sizeof(g_err)) &&
                        fr_bulk_phase_gpu_export_chunk(owners[i], 0, phase, g_err, sizeof(g_err)));
            ASSERT_EQ(memcmp(phase, expected, sizeof(*phase)), 0);
            ASSERT_TRUE(dc_gpu_download_chunk(mono, i, native, g_err, sizeof(g_err)) &&
                        dc_gpu_download_chunk(owners[i], 0, chunk, g_err, sizeof(g_err)));
            ASSERT_EQ(memcmp(native->cells, chunk->cells, sizeof(native->cells)), 0);
            ASSERT_EQ(memcmp(native->face_velocity, chunk->face_velocity, sizeof(native->face_velocity)), 0);
        }
    }
    uint64_t oil = 0, slime = 0;
    for (unsigned i = 0; i < DC_CHUNK_CELLS; ++i) {
        oil += expected->oil_fp[i];
        slime += expected->slime_fp[i];
    }
    ASSERT_TRUE(oil && slime);
    PASS();
cleanup:
    dc_gpu_destroy(mono);
    dc_gpu_destroy(owners[1]);
    dc_gpu_destroy(owners[0]);
    free(native); free(chunk); free(phase); free(expected);
}
static void test_bulk_crosses_the_shared_face_on_every_elapsed_tick(void) { check_bulk_crossing(false); }
static void test_vertical_bulk_exchange_matches_monolithic_ticks(void) { check_bulk_crossing(true); }

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--cadence") == 0) {
        RUN(test_coupled_water_matches_every_native_tick_and_recovers_once);
    } else if (argc == 2 && strcmp(argv[1], "--cadence-clock") == 0) {
        RUN(test_fractional_tick_without_a_phase_keeps_the_queue_idle);
    } else if (argc == 2 && strcmp(argv[1], "--bulk") == 0) {
        RUN(test_bulk_crosses_the_shared_face_on_every_elapsed_tick);
        RUN(test_vertical_bulk_exchange_matches_monolithic_ticks);
    } else if (argc == 1) {
        RUN(test_bulk_crosses_the_shared_face_on_every_elapsed_tick);
        RUN(test_vertical_bulk_exchange_matches_monolithic_ticks);
        RUN(test_fractional_tick_without_a_phase_keeps_the_queue_idle);
        RUN(test_connected_tiles_match_monolithic_water_and_share_one_receipt);
        RUN(test_coupled_water_matches_every_native_tick_and_recovers_once);
    } else return 2;
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
