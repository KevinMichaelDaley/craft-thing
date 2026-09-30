#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../src/app/level.h"
#include "../../src/app/offscreen.h"
#include "../../src/app/session.h"

static int g_pass, g_fail;
#define RUN(fn) do { \
    const char *filter = getenv("DC_TEST_FILTER"); \
    if (!filter || strcmp(filter, #fn) == 0) { \
        printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); \
    } \
} while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_offscreen_cache_uses_compact_gpu_tiles(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 256u, 256u,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_offscreen_t *offscreen = dc_offscreen_create(gpu,
        (dc_chunk_coord_t){0, 0}, err, sizeof(err));
    ASSERT_TRUE(offscreen != NULL);
    ASSERT_EQ(dc_offscreen_slot_capacity(offscreen), 9u);
    dc_offscreen_destroy(offscreen);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_offscreen_catchup_has_bounded_submission_cost(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 256u, 256u,
        "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_offscreen_t *offscreen = dc_offscreen_create(gpu,
        (dc_chunk_coord_t){0, 0}, err, sizeof(err));
    ASSERT_TRUE(offscreen != NULL);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    chunk->cells[5u * DC_CHUNK_SIDE + 32u].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_offscreen_capture(offscreen, chunk, err, sizeof(err)));
    double fastest_ms = 1e9;
    for (uint32_t repeat = 0u; repeat < 3u; ++repeat) {
        struct timespec start, stop;
        ASSERT_TRUE(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
        ASSERT_TRUE(dc_offscreen_update(offscreen, (dc_chunk_coord_t){1, 0},
                                        0.2, true, err, sizeof(err)));
        ASSERT_TRUE(clock_gettime(CLOCK_MONOTONIC, &stop) == 0);
        double elapsed_ms = (stop.tv_sec - start.tv_sec) * 1000.0 +
                            (stop.tv_nsec - start.tv_nsec) / 1e6;
        if (elapsed_ms < fastest_ms) fastest_ms = elapsed_ms;
        ASSERT_TRUE(dc_offscreen_last_advance_ticks(offscreen) > 11.9f);
    }
    printf("fastest offscreen 0.2 s catch-up %.2f ms\n", fastest_ms);
    ASSERT_TRUE(fastest_ms < 12.0);
    free(chunk);
    dc_offscreen_destroy(offscreen);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_camera_crosses_chunk_boundary_cell_by_cell(void) {
    char directory[] = "build/ui_pan_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_level_view_status_t status = {0};
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_EQ(status.ready_chunks, status.total_chunks);
    ASSERT_EQ(status.total_chunks, 24u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 63, 0));
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_INT_EQ(status.origin.x, 0);
    ASSERT_EQ(status.offset_x, 63u);
    ASSERT_TRUE(dc_level_view_paint(view, 0, 5, 0, DC_MATERIAL_STONE,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t color = 0;
    ASSERT_TRUE(dc_level_view_pixel(view, 0, 5, &color, err, sizeof(err)));
    ASSERT_TRUE(color != 0xff181818u);
    uint32_t stone_color = color;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 1, 0));
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_INT_EQ(status.origin.x, 1);
    ASSERT_EQ(status.offset_x, 0u);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 0, 5, 0, DC_MATERIAL_SAND,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 0, 5, &color, err, sizeof(err)));
    ASSERT_TRUE(color != 0xff181818u && color != stone_color);
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[5 * DC_CHUNK_SIDE + 63].material, DC_MATERIAL_STONE);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[5 * DC_CHUNK_SIDE].material, DC_MATERIAL_SAND);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -65, 0));
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_INT_EQ(status.origin.x, -1);
    ASSERT_EQ(status.offset_x, 63u);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 0, 5, 0, DC_MATERIAL_STONE,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){-1, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[5 * DC_CHUNK_SIDE + 63].material, DC_MATERIAL_STONE);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, -1));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 0, 0, 0, DC_MATERIAL_SAND,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){-1, -1},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[63 * DC_CHUNK_SIDE + 63].material,
              DC_MATERIAL_SAND);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_loading_status_and_camera_reset(void) {
    char directory[] = "build/ui_loading_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (int i = 0; i < 7; ++i) ASSERT_TRUE(dc_level_view_move(view, 1, 0));
    dc_level_view_status_t status = {0};
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_TRUE(status.ready_chunks < status.total_chunks);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_EQ(status.ready_chunks, status.total_chunks);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 17, 19));
    ASSERT_TRUE(dc_level_view_reset_camera(view));
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_INT_EQ(status.origin.x, 0);
    ASSERT_EQ(status.offset_x, 0u);
    ASSERT_EQ(status.offset_y, 0u);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_single_step_moves_water_once(void) {
    char directory[] = "build/ui_step_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 80, 5, 3, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    dc_chunk_t *before = calloc(1, sizeof(*before));
    dc_chunk_t *after = calloc(1, sizeof(*after));
    ASSERT_TRUE(before && after);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    before, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    after, err, sizeof(err)));
    ASSERT_EQ(memcmp(before->cells, after->cells, sizeof(before->cells)), 0);
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)) &&
                dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    after, err, sizeof(err)));
    ASSERT_TRUE(memcmp(before->cells, after->cells, sizeof(before->cells)) != 0);
    memcpy(before->cells, after->cells, sizeof(before->cells));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    after, err, sizeof(err)));
    ASSERT_EQ(memcmp(before->cells, after->cells, sizeof(before->cells)), 0);
    free(before); free(after);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_fresh_run_preserves_previous_saved_world(void) {
    char directory[] = "build/ui_reset_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 40, 5, 0, DC_MATERIAL_STONE,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_app_restart_view(&view, directory, 314, 1,
                                    err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[5 * DC_CHUNK_SIDE + 40].material, DC_MATERIAL_AIR);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[5 * DC_CHUNK_SIDE + 40].material, DC_MATERIAL_STONE);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_granular_particle_survives_window_eviction(void) {
    char directory[] = "build/ui_particle_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 40, 5, 0, DC_MATERIAL_SAND,
                                    err, sizeof(err)));
    for (uint32_t tick = 0; tick < 12; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    uint32_t count = chunk->particle_count;
    dc_mpm_particle_t original = {0};
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp &&
            chunk->particles[i].id_lo == 5 * DC_CHUNK_SIDE + 41)
            original = chunk->particles[i];
    ASSERT_EQ(original.mass_fp, DC_FLUID_FULL);
    ASSERT_TRUE(original.y_fp > 5 * (int32_t)DC_FLUID_FULL);
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(dc_level_view_move(view, 1, 0));
        ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    }
    ASSERT_TRUE(!dc_level_view_has_chunk(view, (dc_chunk_coord_t){0, 0}));
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(dc_level_view_move(view, -1, 0));
        ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, count);
    uint32_t preserved = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp &&
            chunk->particles[i].id_lo == original.id_lo &&
            chunk->particles[i].id_hi == original.id_hi &&
            memcmp(&chunk->particles[i], &original, sizeof(original)) == 0)
            ++preserved;
    ASSERT_EQ(preserved, 1u);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_water_keeps_falling_one_screen_offscreen(void) {
    char directory[] = "build/ui_offscreen_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 10, 5, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    uint32_t initial = chunk->cells[5 * DC_CHUNK_SIDE + 10].fluid_mass;
    ASSERT_TRUE(initial > 0u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 300, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 60; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -300, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->cells[5 * DC_CHUNK_SIDE + 10].fluid_mass < initial);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_water_keeps_falling_two_screens_offscreen(void) {
    char directory[] = "build/ui_far_water_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 10, 5, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 600, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -600, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->cells[5 * DC_CHUNK_SIDE + 10].fluid_mass < DC_FLUID_FULL);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_saved_water_resumes_before_returning_to_view(void) {
    char directory[] = "build/ui_returning_water_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 10, 5, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    uint32_t initial = chunk->cells[5 * DC_CHUNK_SIDE + 10].fluid_mass;
    ASSERT_TRUE(initial > 0u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 1024, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 512, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -896, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -640, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->cells[5 * DC_CHUNK_SIDE + 10].fluid_mass < initial);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_water_crosses_visible_offscreen_boundary(void) {
    char directory[] = "build/ui_boundary_flow_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t x = 54; x <= 75; ++x)
        ASSERT_TRUE(dc_level_view_paint(view, x, 10, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    for (uint32_t x = 58; x <= 63; ++x)
        ASSERT_TRUE(dc_level_view_paint(view, x, 9, 0, DC_MATERIAL_WATER,
                                        err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->cells[9 * DC_CHUNK_SIDE].fluid_mass, 0u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 12; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                    chunk, err, sizeof(err)));
    uint64_t crossed = 0;
    for (uint32_t y = 7; y < 10; ++y)
        for (uint32_t x = 0; x < 6; ++x)
            crossed += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    ASSERT_TRUE(crossed > 0u);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_stationary_water_reaches_cold_chunk(void) {
    char directory[] = "build/ui_stationary_front_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t x = 100; x < 255; ++x)
        ASSERT_TRUE(dc_level_view_paint(view, x, 12, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t y = 6; y < 12; ++y)
        for (uint32_t x = 242; x < 256; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_WATER,
                                            err, sizeof(err)));
    ASSERT_TRUE(!dc_level_view_has_chunk(view, (dc_chunk_coord_t){5, 0}));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){5, 0},
                                    chunk, err, sizeof(err)));
    uint64_t mass = 0;
    for (uint32_t y = 0; y < 12; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    ASSERT_TRUE(mass > 0);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_water_crosses_two_offscreen_workspaces(void) {
    char directory[] = "build/ui_workspace_front_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 250, 8, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    for (uint32_t tick = 0; tick < 24; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 512, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t x = 0; x < 220; ++x)
        ASSERT_TRUE(dc_level_view_paint(view, x, 12, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    for (uint32_t y = 6; y < 12; ++y)
        for (uint32_t x = 108; x < 128; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_WATER,
                                            err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -512, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 640, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){10, 0},
                                    chunk, err, sizeof(err)));
    uint64_t mass = 0;
    for (uint32_t y = 0; y < 12; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    ASSERT_TRUE(mass > 0);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static bool deep_water_offscreen(uint64_t *right_mass, uint64_t *far_mass,
                               char *err, uint32_t cap) {
    char directory[] = "build/ui_deep_boundary_XXXXXX";
    if (!mkdtemp(directory)) return false;
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, cap);
    if (!view) return false;
    bool okay = dc_level_view_set_spring_enabled(view, false) &&
                dc_level_view_wait_visible(view, 5000, err, cap) &&
                dc_level_view_pan_pixels(view, 128, 0) &&
                dc_level_view_wait_visible(view, 5000, err, cap);
    for (uint32_t x = 108; x <= 128 && okay; x += 10)
        okay = dc_level_view_paint(view, x, 6, 7, DC_MATERIAL_WATER, err, cap);
    for (uint32_t x = 96; x < 192 && okay; ++x)
        okay = dc_level_view_paint(view, x, 14, 0, DC_MATERIAL_STONE, err, cap);
    if (okay)
        okay = dc_level_view_pan_pixels(view, -128, 0) &&
               dc_level_view_wait_visible(view, 5000, err, cap);
    for (uint32_t tick = 0; tick < 300 && okay; ++tick)
        okay = dc_level_view_step(view, err, cap) &&
               dc_level_view_tick(view, err, cap);
    if (okay)
        okay = dc_level_view_pan_pixels(view, 128, 0) &&
               dc_level_view_wait_visible(view, 5000, err, cap);
    dc_chunk_t *right = calloc(1, sizeof(*right));
    if (!right) okay = false;
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){4, 0},
                                         right, err, cap);
    *right_mass = 0;
    *far_mass = 0;
    if (okay) {
        for (uint32_t y = 0; y < 14; ++y)
            for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
                uint32_t mass = right->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
                *right_mass += mass;
                if (x >= 8u) *far_mass += mass;
            }
    }
    free(right);
    return dc_level_view_destroy(view, err, cap) && okay;
}

static void test_deep_water_crosses_offscreen_boundary_like_visible_water(void) {
    char err[256] = {0};
    uint64_t split = 0, far = 0;
    ASSERT_TRUE(deep_water_offscreen(&split, &far, err, sizeof(err)));
    printf("deep offscreen water after 300 ticks: destination %.2f, eight cells in %.2f\n",
           (double)split / DC_FLUID_FULL, (double)far / DC_FLUID_FULL);
    ASSERT_TRUE(split >= 20u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(far >= 5u * (uint64_t)DC_FLUID_FULL);
    PASS();
}

static void test_two_water_elevations_cross_cold_right_chunks(void) {
    char directory[] = "build/ui_two_wet_rows_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t x = 100; x < 255; ++x) {
        ASSERT_TRUE(dc_level_view_paint(view, x, 12, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, x, 76, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
        for (uint32_t y = 70; y < 76; ++y)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_AIR,
                                            err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t y = 6; y < 12; ++y)
        for (uint32_t x = 242; x < 256; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_WATER,
                                            err, sizeof(err)));
    for (uint32_t y = 70; y < 76; ++y)
        for (uint32_t x = 242; x < 256; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_WATER,
                                            err, sizeof(err)));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 128, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    for (int64_t row = 0; row <= 1; ++row) {
        ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){5, row},
                                        chunk, err, sizeof(err)));
        uint64_t mass = 0;
        for (uint32_t y = 0; y < 12; ++y)
            for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
                mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
        ASSERT_TRUE(mass > 0);
    }
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_falling_water_activates_cold_lower_chunk(void) {
    char directory[] = "build/ui_cold_lower_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, 128));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t y = 0; y < 128; ++y) {
        for (uint32_t x = 30; x <= 34; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_AIR,
                                            err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, 29, y, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, 35, y, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, -128));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t y = 120; y < 128; ++y) {
        for (uint32_t x = 30; x <= 34; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_AIR,
                                            err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, 29, y, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, 35, y, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_paint(view, 32, 120, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    for (uint32_t tick = 0; tick < 180; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, 128));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 3},
                                    chunk, err, sizeof(err)));
    uint64_t mass = 0;
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
        for (uint32_t x = 30; x <= 34; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    ASSERT_TRUE(mass > 0);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_wet_frontier_uses_more_than_four_workspaces(void) {
    char directory[] = "build/ui_deep_frontier_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 250, 8, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)));
    for (uint32_t tick = 0; tick < 24; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 896, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t x = 0; x < 160; ++x)
        ASSERT_TRUE(dc_level_view_paint(view, x, 12, 0, DC_MATERIAL_STONE,
                                        err, sizeof(err)));
    for (uint32_t y = 6; y < 12; ++y)
        for (uint32_t x = 44; x < 64; ++x)
            ASSERT_TRUE(dc_level_view_paint(view, x, y, 0, DC_MATERIAL_WATER,
                                            err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -896, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 120; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 960, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){15, 0},
                                    chunk, err, sizeof(err)));
    uint64_t mass = 0;
    for (uint32_t y = 0; y < 12; ++y)
        for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    ASSERT_TRUE(mass > 0);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_five_distant_wet_regions_stay_gpu_resident(void) {
    char directory[] = "build/ui_five_wet_regions_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    const struct { int32_t pan_x, pan_y; uint32_t paint_x, paint_y; } sites[] = {
        {320, 0, 30, 10}, {-192, 0, 62, 10}, {0, 256, 10, 4},
        {0, -192, 10, 62}, {640, 0, 10, 10}
    };
    for (uint32_t i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i) {
        ASSERT_TRUE(dc_level_view_pan_pixels(view, sites[i].pan_x, sites[i].pan_y));
        ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_paint(view, sites[i].paint_x, sites[i].paint_y,
                                        0, DC_MATERIAL_WATER, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_pan_pixels(view, -sites[i].pan_x,
                                             -sites[i].pan_y));
        ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    }
    for (uint32_t tick = 0; tick < 100; ++tick)
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    dc_level_view_status_t status = {0};
    ASSERT_TRUE(dc_level_view_status(view, &status));
    ASSERT_TRUE(status.offscreen_workspaces > 4u);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_sand_keeps_falling_offscreen_without_particle_loss(void) {
    char directory[] = "build/ui_offscreen_sand_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 10, 5, 0, DC_MATERIAL_SAND,
                                    err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    uint32_t initial_count = chunk->particle_count;
    ASSERT_TRUE(initial_count > 0u);
    dc_mpm_particle_t painted = chunk->particles[5 * DC_CHUNK_SIDE + 10];
    ASSERT_TRUE(painted.mass_fp != 0u);
    int32_t initial_y = painted.y_fp;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 300, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 60; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -300, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    ASSERT_EQ(chunk->particle_count, initial_count);
    int32_t final_y = 0;
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp &&
            chunk->particles[i].id_lo == painted.id_lo &&
            chunk->particles[i].id_hi == painted.id_hi) {
            final_y = chunk->particles[i].y_fp;
            ++found;
        }
    ASSERT_EQ(found, 1u);
    ASSERT_TRUE(final_y > initial_y);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

static void test_sand_crosses_offscreen_visible_boundary_with_same_id(void) {
    char directory[] = "build/ui_boundary_grain_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 713, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t y = 60; y < 96; ++y)
        ASSERT_TRUE(dc_level_view_paint(view, 33, y, 0, DC_MATERIAL_AIR,
                                        err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 33, 63, 0, DC_MATERIAL_SAND,
                                    err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    dc_mpm_particle_t source = chunk->particles[63 * DC_CHUNK_SIDE + 33];
    ASSERT_TRUE(source.mass_fp != 0u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, 128));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 30u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 1},
                                    chunk, err, sizeof(err)));
    uint32_t found = 0;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp &&
            chunk->particles[i].id_lo == source.id_lo &&
            chunk->particles[i].id_hi == source.id_hi) {
            ++found;
        }
    ASSERT_EQ(found, 1u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 0, -128));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 5000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                    chunk, err, sizeof(err)));
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        ASSERT_TRUE(!chunk->particles[i].mass_fp ||
                    chunk->particles[i].id_lo != source.id_lo ||
                    chunk->particles[i].id_hi != source.id_hi);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

int main(void) {
    RUN(test_offscreen_cache_uses_compact_gpu_tiles);
    RUN(test_offscreen_catchup_has_bounded_submission_cost);
    RUN(test_camera_crosses_chunk_boundary_cell_by_cell);
    RUN(test_loading_status_and_camera_reset);
    RUN(test_single_step_moves_water_once);
    RUN(test_fresh_run_preserves_previous_saved_world);
    RUN(test_granular_particle_survives_window_eviction);
    RUN(test_water_keeps_falling_one_screen_offscreen);
    RUN(test_water_keeps_falling_two_screens_offscreen);
    RUN(test_saved_water_resumes_before_returning_to_view);
    RUN(test_water_crosses_visible_offscreen_boundary);
    RUN(test_stationary_water_reaches_cold_chunk);
    RUN(test_water_crosses_two_offscreen_workspaces);
    RUN(test_deep_water_crosses_offscreen_boundary_like_visible_water);
    RUN(test_two_water_elevations_cross_cold_right_chunks);
    RUN(test_falling_water_activates_cold_lower_chunk);
    RUN(test_wet_frontier_uses_more_than_four_workspaces);
    RUN(test_five_distant_wet_regions_stay_gpu_resident);
    RUN(test_sand_keeps_falling_offscreen_without_particle_loss);
    RUN(test_sand_crosses_offscreen_visible_boundary_with_same_id);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
