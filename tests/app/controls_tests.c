#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/app/level.h"
#include "../../src/app/session.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

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
    RUN(test_camera_crosses_chunk_boundary_cell_by_cell);
    RUN(test_loading_status_and_camera_reset);
    RUN(test_single_step_moves_water_once);
    RUN(test_fresh_run_preserves_previous_saved_world);
    RUN(test_granular_particle_survives_window_eviction);
    RUN(test_water_keeps_falling_one_screen_offscreen);
    RUN(test_water_keeps_falling_two_screens_offscreen);
    RUN(test_saved_water_resumes_before_returning_to_view);
    RUN(test_water_crosses_visible_offscreen_boundary);
    RUN(test_sand_keeps_falling_offscreen_without_particle_loss);
    RUN(test_sand_crosses_offscreen_visible_boundary_with_same_id);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
