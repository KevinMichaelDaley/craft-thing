#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "dungeoncraft/chunk.h"
#include "level.h"
#include "session.h"
#include "view_config.h"

static bool save_level_bmp(const char *path, const uint32_t *pixels) {
    const uint32_t width = VIEW_WIDTH * WINDOW_SCALE;
    const uint32_t height = VIEW_HEIGHT * WINDOW_SCALE;
    uint32_t *scaled = malloc((size_t)width * height * sizeof(*scaled));
    if (!scaled) return false;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            scaled[y * width + x] =
                pixels[(y / WINDOW_SCALE) * VIEW_WIDTH + x / WINDOW_SCALE];
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(scaled,
        (int)width, (int)height, 32, (int)(width * sizeof(*scaled)),
        SDL_PIXELFORMAT_ABGR8888);
    bool okay = surface && SDL_SaveBMP(surface, path) == 0;
    if (surface) SDL_FreeSurface(surface);
    free(scaled);
    return okay;
}

static bool paint_held(dc_level_view_t *view, uint16_t material,
                       char *err, uint32_t cap) {
    int mx, my;
    uint32_t buttons = SDL_GetMouseState(&mx, &my);
    uint32_t x, y;
    if (mx < 0 || my < 0 ||
        !dc_level_view_screen_cell(view, (uint32_t)mx, (uint32_t)my, &x, &y))
        return true;
    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))
        return dc_level_view_paint(view, x, y, BRUSH_RADIUS, material, err, cap);
    if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))
        return dc_level_view_paint(view, x, y, BRUSH_RADIUS, DC_MATERIAL_AIR, err, cap);
    return true;
}

#if defined(DC_NATIVE_VIEW) || defined(DC_HALF_NATIVE_VIEW)
static int smoke_native_view(void) {
    char directory[] = "build/ui_native_XXXXXX", err[256] = {0};
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Native create: %s\n", err); return 1; }
    dc_level_view_status_t status = {0};
    uint32_t x = 0, y = 0, painted = 0;
    uint32_t *pixels = malloc((size_t)VIEW_WIDTH * VIEW_HEIGHT * sizeof(*pixels));
    bool okay = pixels && dc_level_view_wait_visible(view, 120000, err, sizeof(err)) &&
                dc_level_view_status(view, &status) &&
                status.total_chunks == DC_GPU_CHUNK_SLOTS &&
                status.ready_chunks == DC_GPU_CHUNK_SLOTS &&
                dc_level_view_screen_cell(view,
                                          VIEW_WIDTH * WINDOW_SCALE - 1u,
                                          VIEW_HEIGHT * WINDOW_SCALE - 1u,
                                          &x, &y) &&
                x == VIEW_WIDTH - 1u && y == VIEW_HEIGHT - 1u &&
                dc_level_view_paint(view, VIEW_WIDTH - 20u, VIEW_HEIGHT - 20u,
                                    2u, DC_MATERIAL_STONE, err, sizeof(err)) &&
                dc_level_view_tick(view, err, sizeof(err)) &&
                dc_level_view_pixel(view, VIEW_WIDTH - 20u,
                                    VIEW_HEIGHT - 20u, &painted,
                                    err, sizeof(err)) &&
                painted == 0xff707070u &&
                dc_level_view_pixels(view, pixels, VIEW_WIDTH * VIEW_HEIGHT,
                                     err, sizeof(err)) &&
                pixels[(VIEW_HEIGHT - 20u) * VIEW_WIDTH + VIEW_WIDTH - 20u] ==
                    0xff707070u;
    mkdir("build/screenshots", 0777);
#ifdef DC_HALF_NATIVE_VIEW
    if (okay) okay = save_level_bmp(
        "build/screenshots/half_native_960x540_upscaled.bmp", pixels);
#else
    if (okay) okay = save_level_bmp("build/screenshots/native_1920x1080.bmp", pixels);
#endif
    uint64_t start = SDL_GetPerformanceCounter();
    double fastest = 1e9, slowest = 0.0;
    for (uint32_t i = 0; i < 12u && okay; ++i)
    {
        uint64_t tick_start = SDL_GetPerformanceCounter();
        okay = dc_level_view_step(view, err, sizeof(err)) &&
               dc_level_view_tick(view, err, sizeof(err));
        double tick_time = (double)(SDL_GetPerformanceCounter() - tick_start) /
                           (double)SDL_GetPerformanceFrequency();
        if (tick_time < fastest) fastest = tick_time;
        if (tick_time > slowest) slowest = tick_time;
    }
    double elapsed = (double)(SDL_GetPerformanceCounter() - start) /
                     (double)SDL_GetPerformanceFrequency();
    double rigid_ms = 0.0, fluid_ms = 0.0, granular_ms = 0.0;
    double fluid_peak_ms = 0.0;
    double fluid_phase_ms[6] = {0};
    for (uint32_t i = 0; i < 6u && okay; ++i) {
        dc_gpu_tick_capture_t capture = {0};
        okay = dc_level_view_capture_tick(view, &capture, err, sizeof(err));
        rigid_ms += capture.stages[0].gpu_ns / 1e6;
        fluid_ms += capture.stages[1].gpu_ns / 1e6;
        fluid_phase_ms[i] = capture.stages[1].gpu_ns / 1e6;
        if (capture.stages[1].gpu_ns / 1e6 > fluid_peak_ms)
            fluid_peak_ms = capture.stages[1].gpu_ns / 1e6;
        granular_ms += capture.stages[2].gpu_ns / 1e6;
    }
    uint64_t adaptive_start = SDL_GetPerformanceCounter();
    uint64_t adaptive_previous = adaptive_start;
    double adaptive_seconds = 0.0;
    for (uint32_t i = 0; i < 12u && okay; ++i) {
        uint64_t now = SDL_GetPerformanceCounter();
        float seconds = i == 0u ? 1.0f / 60.0f :
            (float)((double)(now - adaptive_previous) /
                    (double)SDL_GetPerformanceFrequency());
        if (seconds > 0.25f) seconds = 0.25f;
        adaptive_seconds += seconds;
        adaptive_previous = now;
        okay = dc_level_view_step_timed(view, seconds, err, sizeof(err)) &&
               dc_level_view_tick(view, err, sizeof(err));
    }
    double adaptive_wall = (double)(SDL_GetPerformanceCounter() - adaptive_start) /
                           (double)SDL_GetPerformanceFrequency();
    dc_gpu_memory_stats_t memory_stats = {0};
    if (!dc_level_view_memory_stats(view, &memory_stats)) okay = false;
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(pixels);
    if (!okay) { fprintf(stderr, "Native smoke failed: %s\n", err); return 1; }
    printf("Native viewport %ux%u, simulated cells %u, resident chunks %u, "
           "12 presented physics ticks %.3f s (%.2f ticks/s), "
           "fastest %.1f ms, slowest %.1f ms, fluid updates 2; "
            "GPU stage ms/tick rigid %.1f fluid %.1f (peak %.1f) granular %.1f\n",
           VIEW_WIDTH, VIEW_HEIGHT, VIEW_WIDTH * VIEW_HEIGHT,
           status.ready_chunks, elapsed, 12.0 / elapsed,
           fastest * 1000.0, slowest * 1000.0,
            rigid_ms / 6.0, fluid_ms / 6.0, fluid_peak_ms, granular_ms / 6.0);
    printf("Fluid phase GPU ms: %.1f %.1f %.1f %.1f %.1f %.1f\n",
           fluid_phase_ms[0], fluid_phase_ms[1], fluid_phase_ms[2],
           fluid_phase_ms[3], fluid_phase_ms[4], fluid_phase_ms[5]);
    printf("Adaptive 12 frames %.3f s wall (%.1f frames/s), %.3f s simulated\n",
           adaptive_wall, 12.0 / adaptive_wall, adaptive_seconds);
    printf("GPU buffers: %.1f MiB mapped VRAM, %.1f MiB mapped system, "
           "%.1f MiB device-only VRAM\n",
           memory_stats.mapped_local_bytes / 1048576.0,
           memory_stats.mapped_system_bytes / 1048576.0,
           memory_stats.device_only_bytes / 1048576.0);
    return 0;
}
#endif

static int smoke_moving_water(void) {
    char directory[] = "build/ui_motion_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Motion level create: %s\n", err); return 1; }
    uint32_t *before = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*before));
    uint32_t *after = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*after));
    bool okay = before && after &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_pixels(view, before, VIEW_WIDTH * VIEW_HEIGHT, err, sizeof(err));
    mkdir("build/screenshots", 0777);
    if (okay) okay = save_level_bmp("build/screenshots/before.bmp", before);
    for (uint32_t i = 0; i < 60 && okay; ++i)
        okay = dc_level_view_step(view, err, sizeof(err));
    if (okay) okay = dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_pixels(view, after, VIEW_WIDTH * VIEW_HEIGHT,
                                          err, sizeof(err)) &&
                     save_level_bmp("build/screenshots/after_1s.bmp", after);
    uint32_t changed = 0, deep_changed = 0;
    if (okay) {
        for (uint32_t y = 4; y < 35; ++y)
            for (uint32_t x = 120; x < 138; ++x)
                if (before[y * VIEW_WIDTH + x] != after[y * VIEW_WIDTH + x]) ++changed;
        for (uint32_t y = 20; y < 31; ++y)
            for (uint32_t x = 126; x < 131; ++x)
                if (before[y * VIEW_WIDTH + x] != after[y * VIEW_WIDTH + x])
                    ++deep_changed;
        okay = changed >= 50 && deep_changed >= 10;
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(before);
    free(after);
    if (!okay) {
        fprintf(stderr, "Falling-water screenshot smoke failed (%u changed, %u below source): %s\n",
                changed, deep_changed, err);
        return 1;
    }
    printf("Falling-water screenshots: build/screenshots/before.bmp and after_1s.bmp (%u changed, %u below source)\n",
           changed, deep_changed);
    return 0;
}

static int smoke_granular_fall(void) {
    char directory[] = "build/ui_granular_XXXXXX", err[256] = {0};
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Granular level create: %s\n", err); return 1; }
    uint32_t *before = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*before));
    uint32_t *after = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*after));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = before && after && chunk &&
        dc_level_view_set_spring_enabled(view, false) &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_paint(view, 40, 5, 2, DC_MATERIAL_SAND, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixels(view, before, VIEW_WIDTH * VIEW_HEIGHT, err, sizeof(err)) &&
        dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0}, chunk, err, sizeof(err));
    dc_mpm_particle_t original = {0};
    if (okay) original = chunk->particles[5 * DC_CHUNK_SIDE + 40];
    mkdir("build/screenshots", 0777);
    if (okay) okay = original.mass_fp != 0u &&
                     save_level_bmp("build/screenshots/granular_before.bmp", before);
    for (uint32_t i = 0; i < 60 && okay; ++i)
        okay = dc_level_view_step(view, err, sizeof(err));
    if (okay) okay = dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_pixels(view, after, VIEW_WIDTH * VIEW_HEIGHT,
                                          err, sizeof(err)) &&
                     save_level_bmp("build/screenshots/granular_after_1s.bmp", after) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                         chunk, err, sizeof(err));
    uint32_t changed = 0, moved = 0;
    if (okay) {
        for (uint32_t y = 2; y < 40; ++y)
            for (uint32_t x = 35; x < 46; ++x)
                changed += before[y * VIEW_WIDTH + x] != after[y * VIEW_WIDTH + x];
        for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
            if (chunk->particles[i].mass_fp &&
                chunk->particles[i].id_lo == original.id_lo &&
                chunk->particles[i].id_hi == original.id_hi &&
                chunk->particles[i].y_fp > original.y_fp + 2 * (int32_t)DC_FLUID_FULL)
                ++moved;
        okay = changed >= 10 && moved == 1u;
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(before); free(after); free(chunk);
    if (!okay) {
        fprintf(stderr, "Granular screenshot smoke failed (%u changed, %u moved): %s\n",
                changed, moved, err);
        return 1;
    }
    printf("Granular screenshots: granular_before.bmp and granular_after_1s.bmp (%u changed)\n",
           changed);
    return 0;
}

static bool paint_dirt_floor(dc_level_view_t *view, char *err, uint32_t cap) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    bool okay = true;
    uint32_t loaded_chunk = UINT32_MAX;
    for (uint32_t x = 24; x <= 108 && okay; ++x) {
        uint32_t chunk_x = x / DC_CHUNK_SIDE;
        if (chunk_x != loaded_chunk) {
            okay = dc_level_view_chunk(view, (dc_chunk_coord_t){chunk_x, 0},
                                       chunk, err, cap);
            loaded_chunk = chunk_x;
        }
        for (uint32_t y = 16; y < 60 && okay; ++y) {
            if (chunk->cells[y * DC_CHUNK_SIDE + x % DC_CHUNK_SIDE].material !=
                DC_MATERIAL_STONE) continue;
            okay = dc_level_view_paint(view, x, y, 0, DC_MATERIAL_DIRT,
                                       err, cap);
            break;
        }
    }
    free(chunk);
    return okay;
}

static bool setup_coupled_materials(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!dc_level_view_wait_visible(view, 5000, err, cap) ||
        !paint_dirt_floor(view, err, cap)) return false;
    const uint32_t wall[][2] = {{39, 5}, {41, 5}, {40, 4}, {40, 6}};
    for (uint32_t i = 0; i < 4; ++i)
        if (!dc_level_view_paint(view, wall[i][0], wall[i][1], 0,
                                 DC_MATERIAL_STONE, err, cap)) return false;
    bool painted = dc_level_view_paint(view, 40, 5, 0, DC_MATERIAL_DIRT, err, cap) &&
        dc_level_view_paint(view, 40, 5, 0, DC_MATERIAL_WATER, err, cap) &&
        dc_level_view_paint(view, 50, 5, 1, DC_MATERIAL_SAND, err, cap) &&
        dc_level_view_paint(view, 54, 5, 1, DC_MATERIAL_GRAVEL, err, cap) &&
        dc_level_view_paint(view, 50, 4, 0, DC_MATERIAL_WATER, err, cap) &&
        dc_level_view_paint(view, 80, 8, 3, DC_MATERIAL_SAND, err, cap) &&
        dc_level_view_paint(view, 90, 8, 3, DC_MATERIAL_DIRT, err, cap) &&
        dc_level_view_paint(view, 100, 8, 3, DC_MATERIAL_GRAVEL, err, cap) &&
        dc_level_view_paint(view, 90, 5, 3, DC_MATERIAL_WATER, err, cap);
    if (!painted) return false;
    for (uint32_t y = 3; y <= 7; ++y)
        for (uint32_t x = 59; x <= 63; ++x) {
            bool border = x == 59 || x == 63 || y == 3 || y == 7;
            if (!dc_level_view_paint(view, x, y, 0,
                    border ? DC_MATERIAL_STONE : DC_MATERIAL_DIRT, err, cap))
                return false;
            if (!border && !dc_level_view_paint(view, x, y, 0,
                    DC_MATERIAL_WATER, err, cap)) return false;
        }
    return true;
}

enum { SIFT_LEFT = 72, SIFT_RIGHT = 80, SIFT_FIRST = 73, SIFT_LAST = 79,
       SIFT_TOP = 8, SIFT_BOTTOM = 17, SIFT_FLOOR = 45 };

static bool setup_sifting_materials(dc_level_view_t *view, char *err, uint32_t cap) {
    if (!dc_level_view_wait_visible(view, 5000, err, cap)) return false;
    for (uint32_t x = SIFT_LEFT; x <= SIFT_RIGHT; ++x)
        if (!dc_level_view_paint(view, x, SIFT_FLOOR, 0, DC_MATERIAL_STONE, err, cap))
            return false;
    for (uint32_t y = SIFT_BOTTOM; y < SIFT_FLOOR; ++y)
        if (!dc_level_view_paint(view, SIFT_LEFT, y, 0, DC_MATERIAL_STONE, err, cap) ||
            !dc_level_view_paint(view, SIFT_RIGHT, y, 0, DC_MATERIAL_STONE, err, cap))
            return false;
    const uint16_t kinds[] = {DC_MATERIAL_SAND, DC_MATERIAL_DIRT,
                              DC_MATERIAL_GRAVEL};
    for (uint32_t y = SIFT_TOP; y < SIFT_BOTTOM; ++y)
        for (uint32_t x = SIFT_FIRST; x <= SIFT_LAST; ++x)
            if (!dc_level_view_paint(view, x, y, 0, kinds[(x + y) % 3u],
                                     err, cap)) return false;
    return true;
}

static int smoke_sifting_materials(void) {
    char directory[] = "build/ui_sifting_XXXXXX", err[256] = {0};
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Sifting level create: %s\n", err); return 1; }
    uint32_t *before = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*before));
    uint32_t *after = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*after));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = before && after && chunk &&
        dc_level_view_set_spring_enabled(view, false) &&
        setup_sifting_materials(view, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixels(view, before, VIEW_WIDTH * VIEW_HEIGHT, err, sizeof(err));
    mkdir("build/screenshots", 0777);
    if (okay) okay = save_level_bmp("build/screenshots/sifting_before.bmp", before);
    for (uint32_t tick = 0; tick < 60 && okay; ++tick)
        okay = dc_level_view_step(view, err, sizeof(err));
    if (okay) okay = dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_pixels(view, after, VIEW_WIDTH * VIEW_HEIGHT,
                                          err, sizeof(err)) &&
                     save_level_bmp("build/screenshots/sifting_after_1s.bmp", after) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         chunk, err, sizeof(err));
    uint64_t depth[3] = {0};
    uint32_t count[3] = {0}, changed = 0;
    if (okay) {
        for (uint32_t y = SIFT_TOP; y < SIFT_FLOOR; ++y)
            for (uint32_t x = SIFT_LEFT; x <= SIFT_RIGHT; ++x)
                changed += before[y * VIEW_WIDTH + x] != after[y * VIEW_WIDTH + x];
        uint32_t seed = dc_chunk_particle_seed((dc_chunk_coord_t){1, 0});
        for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
            const dc_mpm_particle_t *p = &chunk->particles[i];
            if (!p->mass_fp || p->id_hi != seed) continue;
            uint32_t source = p->id_lo - 1u;
            if (source / DC_CHUNK_SIDE < SIFT_TOP ||
                source / DC_CHUNK_SIDE >= SIFT_BOTTOM ||
                source % DC_CHUNK_SIDE < SIFT_FIRST - DC_CHUNK_SIDE ||
                source % DC_CHUNK_SIDE > SIFT_LAST - DC_CHUNK_SIDE)
                continue;
            uint32_t kind = p->material == DC_MATERIAL_SAND ? 0u :
                            p->material == DC_MATERIAL_DIRT ? 1u : 2u;
            ++count[kind];
            depth[kind] += (uint32_t)p->y_fp;
            if (p->x_fp <= (int32_t)(SIFT_LEFT - DC_CHUNK_SIDE) * (int32_t)DC_FLUID_FULL ||
                p->x_fp >= (int32_t)(SIFT_RIGHT - DC_CHUNK_SIDE) * (int32_t)DC_FLUID_FULL ||
                p->y_fp >= SIFT_FLOOR * (int32_t)DC_FLUID_FULL) okay = false;
        }
        okay = okay && changed >= 60u && count[0] == 21u &&
               count[1] == 21u && count[2] == 21u &&
               depth[0] > depth[2] + count[0] * (DC_FLUID_FULL / 2u);
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(before); free(after); free(chunk);
    if (!okay) {
        fprintf(stderr, "Sifting screenshot smoke failed (%u changed, counts %u/%u/%u, depths %llu/%llu/%llu): %s\n",
                changed, count[0], count[1], count[2],
                (unsigned long long)depth[0], (unsigned long long)depth[1],
                (unsigned long long)depth[2], err);
        return 1;
    }
    printf("Sifting screenshots: sifting_before.bmp and sifting_after_1s.bmp "
           "(%u changed, sand/gravel depth gap %.2f cells)\n", changed,
           (double)(depth[0] - depth[2]) / (21.0 * DC_FLUID_FULL));
    return 0;
}

static int smoke_coupled_materials(void) {
    char directory[] = "build/ui_coupled_XXXXXX", err[256] = {0};
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Coupled level create: %s\n", err); return 1; }
    uint32_t *before = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*before));
    uint32_t *after = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*after));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    bool okay = before && after && chunk &&
        dc_level_view_set_spring_enabled(view, false) &&
        setup_coupled_materials(view, err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixels(view, before, VIEW_WIDTH * VIEW_HEIGHT, err, sizeof(err)) &&
        dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0}, chunk, err, sizeof(err));
    uint32_t dirt_id = 0, sand_id = 0, gravel_id = 0;
    uint32_t dirt_floor_samples = 0;
    if (okay) {
        dirt_id = chunk->particles[5 * DC_CHUNK_SIDE + 40].id_lo;
        sand_id = chunk->particles[5 * DC_CHUNK_SIDE + 50].id_lo;
        gravel_id = chunk->particles[5 * DC_CHUNK_SIDE + 54].id_lo;
        for (uint32_t x = 30; x < 64; x += 8)
            for (uint32_t y = 16; y < 60; ++y) {
                uint16_t material = chunk->cells[y * DC_CHUNK_SIDE + x].material;
                if (material == DC_MATERIAL_AIR) continue;
                dirt_floor_samples += material == DC_MATERIAL_DIRT;
                break;
            }
        okay = dirt_id && sand_id && gravel_id && dirt_floor_samples >= 4u;
    }
    mkdir("build/screenshots", 0777);
    if (okay) okay = save_level_bmp("build/screenshots/coupled_before.bmp", before);
    for (uint32_t i = 0; i < 60 && okay; ++i)
        okay = dc_level_view_step(view, err, sizeof(err));
    if (okay) okay = dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_pixels(view, after, VIEW_WIDTH * VIEW_HEIGHT,
                                          err, sizeof(err)) &&
                     save_level_bmp("build/screenshots/coupled_after_1s.bmp", after) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){0, 0},
                                         chunk, err, sizeof(err));
    uint32_t moved = 0, muddy = 0, fragmented = 0, changed = 0;
    if (okay) {
        for (uint32_t y = 3; y < 40; ++y)
            for (uint32_t x = 35; x < 108; ++x)
                changed += before[y * VIEW_WIDTH + x] != after[y * VIEW_WIDTH + x];
        for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i) {
            dc_mpm_particle_t *particle = &chunk->particles[i];
            if (!particle->mass_fp) continue;
            if (particle->material == DC_MATERIAL_DIRT &&
                (particle->flags & DC_MPM_MUD_FLAG) != 0u) ++muddy;
            if (particle->id_lo == dirt_id && particle->material == DC_MATERIAL_DIRT &&
                (particle->flags & DC_MPM_FRAGMENT_FLAG) != 0u) ++fragmented;
            if ((particle->id_lo == sand_id || particle->id_lo == gravel_id) &&
                particle->y_fp > 7 * (int32_t)DC_FLUID_FULL) ++moved;
        }
        okay = changed >= 60 && muddy >= 8u && fragmented == 1u && moved == 2u;
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(before); free(after); free(chunk);
    if (!okay) {
        fprintf(stderr, "Coupled screenshot smoke failed (%u changed, %u mud, %u fragments, %u moved, %u dirt floor): %s\n",
                changed, muddy, fragmented, moved, dirt_floor_samples, err);
        return 1;
    }
    printf("Coupled screenshots: coupled_before.bmp and coupled_after_1s.bmp "
           "(%u changed, %u mud, %u fragments, %u moved)\n",
           changed, muddy, fragmented, moved);
    return 0;
}

static int smoke_moving_water_long(void) {
    enum { FIVE_SECONDS = 300, TEN_SECONDS = 600, PAINT_TICK = 361,
           FRAME_DELAY_MS = 16 };
    char directory[] = "build/ui_motion_long_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Long motion level create: %s\n", err); return 1; }
    uint32_t *at_five = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*at_five));
    uint32_t *at_ten = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*at_ten));
    bool okay = at_five && at_ten &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    uint16_t material = DC_MATERIAL_WATER;
    mkdir("build/screenshots", 0777);
    for (uint32_t tick = 1; tick <= TEN_SECONDS && okay; ++tick) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type != SDL_KEYDOWN) continue;
            switch (event.key.keysym.sym) {
            case SDLK_0: material = DC_MATERIAL_AIR; break;
            case SDLK_1: material = DC_MATERIAL_STONE; break;
            case SDLK_2: material = DC_MATERIAL_SAND; break;
            case SDLK_3: material = DC_MATERIAL_WATER; break;
            case SDLK_4: material = DC_MATERIAL_DIRT; break;
            case SDLK_5: material = DC_MATERIAL_GRAVEL; break;
            case SDLK_m: dc_level_view_toggle_marker_overlay(view); break;
            default: break;
            }
        }
        okay = paint_held(view, material, err, sizeof(err));
        if (tick == PAINT_TICK)
            okay = dc_level_view_paint(view, 190, 12, 5, DC_MATERIAL_WATER,
                                       err, sizeof(err));
        if (okay) okay = dc_level_view_step(view, err, sizeof(err)) &&
                         dc_level_view_tick(view, err, sizeof(err));
        if (okay && tick == FIVE_SECONDS)
            okay = dc_level_view_pixels(view, at_five, VIEW_WIDTH * VIEW_HEIGHT,
                                        err, sizeof(err)) &&
                   save_level_bmp("build/screenshots/after_5s.bmp", at_five);
        if (okay && tick == TEN_SECONDS)
            okay = dc_level_view_pixels(view, at_ten, VIEW_WIDTH * VIEW_HEIGHT,
                                        err, sizeof(err)) &&
                   save_level_bmp("build/screenshots/after_10s.bmp", at_ten);
        SDL_Delay(FRAME_DELAY_MS);
    }
    uint32_t flow_changed = 0, edit_changed = 0;
    uint32_t overfull = 0, maximum_mass = 0;
    uint64_t total_mass = 0;
    for (uint32_t cy = 0; cy < 2 && okay; ++cy)
        for (uint32_t cx = 0; cx < 4 && okay; ++cx) {
            dc_chunk_t chunk = {0};
            okay = dc_level_view_chunk(view, (dc_chunk_coord_t){cx, cy},
                                       &chunk, err, sizeof(err));
            for (uint32_t i = 0; i < DC_CHUNK_CELLS && okay; ++i) {
                uint32_t mass = chunk.cells[i].fluid_mass;
                total_mass += mass;
                if (mass > maximum_mass) maximum_mass = mass;
                if (mass > DC_FLUID_FULL) ++overfull;
            }
        }
    printf("fluid volume cells=%llu overfull=%u maximum=%u\n",
           (unsigned long long)(total_mass / DC_FLUID_FULL), overfull, maximum_mass);
    if (overfull != 0) okay = false;
    if (okay) {
        for (uint32_t y = 0; y < VIEW_HEIGHT; ++y)
            for (uint32_t x = 0; x < VIEW_WIDTH; ++x)
                if (at_five[y * VIEW_WIDTH + x] != at_ten[y * VIEW_WIDTH + x]) {
                    if (x >= 180 && x <= 200 && y <= 40) ++edit_changed;
                    else ++flow_changed;
                }
        okay = flow_changed >= 50 && edit_changed >= 10;
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    free(at_five);
    free(at_ten);
    if (!okay) {
        fprintf(stderr, "Long fluid smoke failed (%u flow, %u near brush): %s\n",
                flow_changed, edit_changed, err);
        return 1;
    }
    printf("Long fluid screenshots: after_5s.bmp and after_10s.bmp "
           "(%u flow, %u near brush)\n", flow_changed, edit_changed);
    return 0;
}

static int smoke_streamed_level(void) {
    char directory[] = "build/ui_stream_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Level create: %s\n", err); return 1; }
    const char *stage = "initial load";
    bool okay = dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) stage = "offscreen halo residency";
    if (okay) {
        bool top_left = dc_level_view_has_chunk(view, (dc_chunk_coord_t){-1, -1});
        bool bottom_right = dc_level_view_has_chunk(view, (dc_chunk_coord_t){4, 2});
        printf("offscreen halo corners resident=%d,%d\n", top_left, bottom_right);
        okay = top_left && bottom_right;
    }
    if (okay) stage = "paint and persist halo edge";
    if (okay) okay = dc_level_view_paint(view, 0, 5, 3, DC_MATERIAL_STONE,
                                         err, sizeof(err));
    dc_chunk_t halo_painted = {0};
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){-1, 0},
                                         &halo_painted, err, sizeof(err)) &&
                     halo_painted.cells[5 * DC_CHUNK_SIDE + 63].material ==
                         DC_MATERIAL_STONE;
    const char *rigid_stage = "rigid spawn";
    if (okay) okay = dc_level_view_spawn_body(view, 63, 2, err, sizeof(err));
    rigid_stage = "rigid cross-chunk step";
    if (okay) okay = dc_level_view_step(view, err, sizeof(err)) &&
                     dc_level_view_tick(view, err, sizeof(err));
    uint32_t rigid_color = 0;
    if (okay) okay = dc_level_view_pixel(view, 64, 2, &rigid_color, err, sizeof(err)) &&
                     ((rigid_color >> 8) & 255u) > 100u &&
                     ((rigid_color >> 8) & 255u) > (rigid_color & 255u);
    if (!okay) stage = rigid_stage;
    if (okay) stage = "falling spring";
    dc_chunk_t falling = {0};
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){2, 0},
                                         &falling, err, sizeof(err));
    uint64_t falling_mass = 0;
    for (uint32_t y = 5; y <= 8; ++y)
        for (uint32_t x = 0; x <= 2; ++x)
            falling_mass += falling.cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    if (okay) okay = falling_mass > 0;
    uint32_t color = 0;
    if (okay) stage = "basin pixel";
    if (okay) okay = dc_level_view_pixel(view, 128, 40, &color, err, sizeof(err)) &&
                     color == 0xffd07030u;
    if (okay) stage = "positive paint";
    if (okay) okay = dc_level_view_paint(view, 64, 5, 0, DC_MATERIAL_SAND,
                                         err, sizeof(err));
    dc_chunk_t chunk = {0};
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND;
    if (okay) stage = "marker paint";
    if (okay) okay = dc_level_view_paint(view, 74, 5, 1, DC_MATERIAL_WATER,
                                         err, sizeof(err)) &&
                     dc_level_view_step(view, err, sizeof(err)) &&
                     dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.marker_count > 0;
    uint32_t saved_marker_count = chunk.marker_count;
    dc_face_velocity_t saved_faces[DC_CHUNK_CELLS];
    memcpy(saved_faces, chunk.face_velocity, sizeof(saved_faces));
    bool moving_face = false;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
        if (saved_faces[i].x != 0.0f || saved_faces[i].y != 0.0f)
            moving_face = true;
    if (okay) okay = moving_face;
    if (okay) stage = "negative load";
    if (okay) okay = dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) stage = "negative paint";
    if (okay) okay = dc_level_view_paint(view, 63, 5, 0, DC_MATERIAL_STONE,
                                         err, sizeof(err));
    if (okay) stage = "negative eviction";
    if (okay) okay = dc_level_view_move(view, 1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) stage = "negative reload";
    if (okay) okay = dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){-1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE + 63].material == DC_MATERIAL_STONE;
    if (okay) stage = "positive eviction";
    if (okay) okay = dc_level_view_move(view, 1, 0) && dc_level_view_move(view, 1, 0) &&
                     dc_level_view_move(view, 1, 0) && dc_level_view_move(view, 1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    uint64_t deadline = SDL_GetTicks64() + 5000;
    while (okay && dc_level_view_has_chunk(view, (dc_chunk_coord_t){1, 0}) &&
           SDL_GetTicks64() < deadline) {
        okay = dc_level_view_tick(view, err, sizeof(err));
        SDL_Delay(1);
    }
    if (okay) okay = !dc_level_view_has_chunk(view, (dc_chunk_coord_t){1, 0});
    if (okay) stage = "positive reload";
    if (okay) okay = dc_level_view_move(view, -1, 0) && dc_level_view_move(view, -1, 0) &&
                     dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND &&
                     chunk.marker_count == saved_marker_count &&
                     memcmp(saved_faces, chunk.face_velocity,
                            sizeof(saved_faces)) == 0;
    if (okay) stage = "halo edge reload";
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){-1, 0},
                                         &halo_painted, err, sizeof(err)) &&
                     halo_painted.cells[5 * DC_CHUNK_SIDE + 63].material ==
                         DC_MATERIAL_STONE;
    bool closed = dc_level_view_destroy(view, err, sizeof(err));
    if (okay && closed) {
        stage = "process restart velocity reload";
        view = dc_level_view_create(directory, 314, err, sizeof(err));
        okay = view && dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
               dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                   &chunk, err, sizeof(err)) &&
               memcmp(saved_faces, chunk.face_velocity,
                      sizeof(saved_faces)) == 0;
        if (view && !dc_level_view_destroy(view, err, sizeof(err))) closed = false;
    }
    if (!okay || !closed) {
        fprintf(stderr, "Streamed level smoke failed at %s: %s\n", stage, err);
        return 1;
    }
    printf("Streamed interactive Vulkan level smoke passed\n");
    return 0;
}

static int smoke_world_transfer(void) {
    char directory[] = "build/ui_transfer_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "%s\n", err); return 1; }
    dc_chunk_t *source = calloc(1, sizeof(*source));
    dc_chunk_t *destination = calloc(1, sizeof(*destination));
    bool okay = source && destination &&
        dc_level_view_set_spring_enabled(view, false) &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_move(view, 1, 0) &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_paint(view, 255, 5, 0, DC_MATERIAL_SAND, err, sizeof(err)) &&
        dc_level_view_move(view, -1, 0) &&
        dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_queue_transfer(view, 319, 5, 320, 5, 0,
                                     DC_GPU_TRANSFER_PARTICLE, err, sizeof(err));
    dc_gpu_transfer_state_t state = DC_GPU_TRANSFER_PENDING;
    if (okay) okay = !dc_level_view_transfer_result(view, &state);
    for (int i = 0; okay && i < 7; ++i)
        okay = dc_level_view_move(view, 1, 0);
    if (okay) okay = dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    for (int i = 0; okay && i < 100 && !dc_level_view_transfer_result(view, &state); ++i)
        okay = dc_level_view_tick(view, err, sizeof(err));
    for (int i = 0; okay && i < 6; ++i)
        okay = dc_level_view_move(view, -1, 0);
    if (okay) okay = dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) okay = state == DC_GPU_TRANSFER_APPLIED &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){4, 0}, source,
                                         err, sizeof(err)) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){5, 0}, destination,
                                         err, sizeof(err)) &&
                     source->cells[5 * DC_CHUNK_SIDE + 63].material == DC_MATERIAL_AIR &&
                     destination->cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND;
    if (okay) okay = dc_level_view_tick(view, err, sizeof(err)) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){5, 0}, destination,
                                         err, sizeof(err)) &&
                     destination->cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND;
    if (okay) okay = dc_level_view_queue_transfer(view, 320, 5, 319, 5, 0,
                                                  DC_GPU_TRANSFER_PARTICLE,
                                                  err, sizeof(err));
    free(source); free(destination);
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    if (!okay) fprintf(stderr, "World transfer smoke failed: %s\n", err);
    else printf("World-anchored streamed transfer smoke passed\n");
    return okay ? 0 : 1;
}

static int smoke_display(void) {
    char directory[] = "build/ui_display_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "%s\n", err); return 1; }
    uint32_t x = UINT32_MAX, y = UINT32_MAX, natural = 0, border = 0, stage = 0;
    bool okay = dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
        dc_level_view_set_zoom(view, 1) &&
        !dc_level_view_set_zoom(view, 3) &&
        dc_level_view_screen_cell(view, 384, 192, &x, &y) && x == 0 && y == 0 &&
        dc_level_view_screen_cell(view, 639, 319, &x, &y) && x == 255 && y == 127 &&
        !dc_level_view_screen_cell(view, 383, 192, &x, &y) &&
        dc_level_view_set_zoom(view, 2) &&
        dc_level_view_screen_cell(view, 767, 383, &x, &y) && x == 255 && y == 127 &&
        dc_level_view_set_zoom(view, 4) &&
        dc_level_view_screen_cell(view, 1023, 511, &x, &y) && x == 255 && y == 127 &&
        dc_level_view_pixel(view, 0, 0, &natural, err, sizeof(err)) &&
        dc_level_view_set_overlay(view, DC_GPU_OVERLAY_RESIDENCY) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixel(view, 0, 0, &border, err, sizeof(err)) &&
        border == 0xff30d030u && border != natural &&
        dc_level_view_set_overlay(view, DC_GPU_OVERLAY_STAGES) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixel(view, 2, 2, &stage, err, sizeof(err)) &&
        stage == 0xff30c040u &&
        dc_level_view_paint(view, 40, 5, 0, DC_MATERIAL_WATER,
                            err, sizeof(err)) &&
        dc_level_view_tick(view, err, sizeof(err)) &&
        dc_level_view_pixel(view, 40, 5, &stage, err, sizeof(err)) &&
        stage == 0xffd08030u;
    if (okay) okay = dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
                     dc_level_view_set_zoom(view, 1) &&
                     dc_level_view_screen_cell(view, 384, 197, &x, &y) &&
                     x == 0 && y == 5 &&
                     dc_level_view_paint(view, x, y, 0, DC_MATERIAL_STONE,
                                         err, sizeof(err));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (okay) okay = chunk && dc_level_view_chunk(view,
        (dc_chunk_coord_t){-1, 0}, chunk, err, sizeof(err)) &&
        chunk->cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_STONE;
    free(chunk);
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    if (!okay) fprintf(stderr, "Display smoke failed: %s\n", err);
    else printf("Vulkan zoom and GPU overlays smoke passed\n");
    return okay ? 0 : 1;
}

static int smoke_halo_flow(void) {
    char directory[] = "build/ui_halo_flow_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Halo level create: %s\n", err); return 1; }
    bool okay = dc_level_view_set_spring_enabled(view, false) &&
                dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
                dc_level_view_move(view, 0, -1) &&
                dc_level_view_wait_visible(view, 5000, err, sizeof(err)) &&
                dc_level_view_paint(view, 80, 61, 0, DC_MATERIAL_WATER,
                                    err, sizeof(err)) &&
                dc_level_view_move(view, 0, 1) &&
                dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    for (uint32_t tick = 0; tick < 12 && okay; ++tick)
        okay = dc_level_view_step(view, err, sizeof(err)) &&
               dc_level_view_tick(view, err, sizeof(err));
    dc_chunk_t upper = {0}, lower = {0};
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, -1},
                                         &upper, err, sizeof(err)) &&
                     dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &lower, err, sizeof(err));
    uint64_t above = 0, visible = 0;
    for (uint32_t y = 0; y < DC_CHUNK_SIDE && okay; ++y)
        for (uint32_t x = 0; x < 32; ++x) {
            above += upper.cells[y * DC_CHUNK_SIDE + x].fluid_mass;
            visible += lower.cells[y * DC_CHUNK_SIDE + x].fluid_mass;
        }
    okay = okay && above + visible == DC_FLUID_FULL && visible > 0;
    if (!dc_level_view_destroy(view, err, sizeof(err))) okay = false;
    if (!okay) {
        fprintf(stderr, "Halo flow failed (upper=%llu visible=%llu): %s\n",
                (unsigned long long)above, (unsigned long long)visible, err);
        return 1;
    }
    printf("Offscreen water entered camera with exact mass\n");
    return 0;
}

static bool camera_velocity_variant(bool pan, dc_chunk_t *result,
                                    char *err, uint32_t cap) {
    char directory[] = "build/ui_camera_velocity_XXXXXX";
    if (!mkdtemp(directory)) return false;
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, cap);
    if (!view) return false;
    bool okay = dc_level_view_set_spring_enabled(view, false) &&
                dc_level_view_wait_visible(view, 5000, err, cap) &&
                dc_level_view_paint(view, 80, 5, 3, DC_MATERIAL_WATER, err, cap);
    for (uint32_t tick = 0; tick < 8 && okay; ++tick)
        okay = dc_level_view_step(view, err, cap) &&
               dc_level_view_tick(view, err, cap);
    if (pan && okay)
        okay = dc_level_view_move(view, 1, 0) &&
               dc_level_view_wait_visible(view, 5000, err, cap);
    for (uint32_t tick = 0; tick < 12 && okay; ++tick)
        okay = dc_level_view_step(view, err, cap) &&
               dc_level_view_tick(view, err, cap);
    if (pan && okay)
        okay = dc_level_view_move(view, -1, 0) &&
               dc_level_view_wait_visible(view, 5000, err, cap);
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         result, err, cap);
    return dc_level_view_destroy(view, err, cap) && okay;
}

static int smoke_camera_velocity(void) {
    char err[256] = {0};
    dc_chunk_t stable = {0}, panned = {0};
    bool okay = camera_velocity_variant(false, &stable, err, sizeof(err)) &&
                camera_velocity_variant(true, &panned, err, sizeof(err));
    uint32_t different = 0;
    for (uint32_t i = 0; i < DC_CHUNK_CELLS && okay; ++i)
        if (stable.cells[i].fluid_mass != panned.cells[i].fluid_mass) ++different;
    if (!okay || different != 0) {
        fprintf(stderr, "Camera velocity replay failed (%u different cells): %s\n",
                different, err);
        return 1;
    }
    printf("Camera move preserved fluid evolution cell-for-cell\n");
    return 0;
}

static bool restart_demo(dc_level_view_t **view, const char *base_directory,
                         uint64_t seed, uint32_t *run, uint32_t zoom,
                         dc_gpu_overlay_t overlay, bool spring_enabled,
                         bool marker_overlay, char *err, uint32_t cap) {
    if (*run == UINT32_MAX) {
        snprintf(err, cap, "Too many world restarts");
        return false;
    }
    ++*run;
    if (!dc_app_restart_view(view, base_directory, seed, *run, err, cap))
        return false;
    if (!dc_level_view_set_zoom(*view, zoom) ||
        !dc_level_view_set_overlay(*view, overlay) ||
        !dc_level_view_set_spring_enabled(*view, spring_enabled) ||
        (marker_overlay && !dc_level_view_toggle_marker_overlay(*view))) {
        snprintf(err, cap, "Cannot restore world display settings");
        return false;
    }
    return true;
}

static bool update_demo_title(dc_level_view_t *view, uint64_t seed,
                              uint32_t zoom, bool paused, bool entering_seed,
                              bool seed_error, const char *seed_text,
                              char previous[192]) {
    dc_level_view_status_t status;
    if (!dc_level_view_status(view, &status)) return false;
    char title[192];
    if (entering_seed)
        snprintf(title, sizeof(title), "%sSeed: %s  |  Enter to load, Esc to cancel",
                 seed_error ? "Invalid seed  |  " : "", seed_text);
    else
        snprintf(title, sizeof(title),
                 "Dungeoncraft  |  seed %" PRIu64 "  |  chunk (%" PRId64 ",%" PRId64
                 ") + (%u,%u)  |  loaded %u/%u  |  %ux%s",
                 seed, status.origin.x, status.origin.y, status.offset_x,
                 status.offset_y, status.ready_chunks, status.total_chunks,
                 zoom, paused ? "  |  paused" : "");
    if (strcmp(previous, title) == 0) return true;
    if (!dc_level_view_set_title(view, title)) return false;
    memcpy(previous, title, strlen(title) + 1);
    return true;
}

static bool pan_held_keys(dc_level_view_t *view, double elapsed,
                          double *carry_x, double *carry_y) {
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    int32_t horizontal = (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) -
                         (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
    int32_t vertical = (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) -
                       (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]);
    double speed = horizontal && vertical ? 169.705627 : 240.0;
    *carry_x += (double)horizontal * speed * elapsed;
    *carry_y += (double)vertical * speed * elapsed;
    int32_t dx = (int32_t)*carry_x, dy = (int32_t)*carry_y;
    *carry_x -= dx;
    *carry_y -= dy;
    return !dx && !dy ? true : dc_level_view_pan_pixels(view, dx, dy);
}

int main(int argc, char **argv) {
#if defined(DC_NATIVE_VIEW) || defined(DC_HALF_NATIVE_VIEW)
    if (argc > 1 && strcmp(argv[1], "--smoke-native") == 0)
        return smoke_native_view();
#endif
    if (argc > 1 && strcmp(argv[1], "--smoke-camera-velocity") == 0)
        return smoke_camera_velocity();
    if (argc > 1 && strcmp(argv[1], "--smoke-halo-flow") == 0)
        return smoke_halo_flow();
    if (argc > 1 && strcmp(argv[1], "--smoke-stream") == 0)
        return smoke_streamed_level();
    if (argc > 1 && strcmp(argv[1], "--smoke-world-transfer") == 0)
        return smoke_world_transfer();
    if (argc > 1 && strcmp(argv[1], "--smoke-display") == 0)
        return smoke_display();
    if (argc > 1 && strcmp(argv[1], "--smoke-motion") == 0)
        return smoke_moving_water();
    if (argc > 1 && strcmp(argv[1], "--smoke-granular") == 0)
        return smoke_granular_fall();
    if (argc > 1 && strcmp(argv[1], "--smoke-coupled") == 0)
        return smoke_coupled_materials();
    if (argc > 1 && strcmp(argv[1], "--smoke-sifting") == 0)
        return smoke_sifting_materials();
    if (argc > 1 && strcmp(argv[1], "--smoke-motion-long") == 0)
        return smoke_moving_water_long();
    bool scripted_input = argc > 1 && strcmp(argv[1], "--smoke-controls-ui") == 0;
    bool demo_coupled = false, demo_sifting = false;
    char scripted_directory[] = "build/ui_input_XXXXXX";
    if (scripted_input && !mkdtemp(scripted_directory)) {
        perror("mkdtemp");
        return 1;
    }
    uint64_t seed = 314;
#ifdef DC_HALF_NATIVE_VIEW
    const char *directory = scripted_input ? scripted_directory : "world_chunks_half_native";
#elif defined(DC_NATIVE_VIEW)
    const char *directory = scripted_input ? scripted_directory : "world_chunks_native";
#else
    const char *directory = scripted_input ? scripted_directory : "world_chunks";
#endif
    for (int i = 1; i < argc; ++i) {
        if (scripted_input && strcmp(argv[i], "--smoke-controls-ui") == 0)
            continue;
        if (strcmp(argv[i], "--demo-coupled") == 0) {
            demo_coupled = true;
            continue;
        }
        if (strcmp(argv[i], "--demo-sifting") == 0) {
            demo_sifting = true;
            continue;
        }
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--world-dir") == 0 && i + 1 < argc)
            directory = argv[++i];
        else {
            fprintf(stderr, "Usage: %s [--seed number] [--world-dir path] [--demo-coupled|--demo-sifting]\n", argv[0]);
            return 1;
        }
    }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, seed, err, sizeof(err));
    if (!view) { fprintf(stderr, "Level create: %s\n", err); return 1; }
    if (demo_coupled &&
        (!dc_level_view_set_spring_enabled(view, false) ||
         !setup_coupled_materials(view, err, sizeof(err)))) {
        fprintf(stderr, "Coupled demo setup: %s\n", err);
        dc_level_view_destroy(view, err, sizeof(err));
        return 1;
    }
    if (demo_sifting &&
        (!dc_level_view_set_spring_enabled(view, false) ||
         !setup_sifting_materials(view, err, sizeof(err)))) {
        fprintf(stderr, "Sifting demo setup: %s\n", err);
        dc_level_view_destroy(view, err, sizeof(err));
        return 1;
    }
    uint16_t material = DC_MATERIAL_SAND;
    bool running = true;
    bool failed = false;
    bool paused = false;
    bool spring_enabled = !(demo_coupled || demo_sifting);
    bool marker_overlay = false;
    bool single_step = false;
    uint32_t zoom = WINDOW_SCALE;
    uint32_t run = 0;
    dc_gpu_overlay_t overlay = DC_GPU_OVERLAY_NONE;
    bool entering_seed = false, seed_error = false;
    char seed_text[21] = {0}, previous_title[192] = {0};
    size_t seed_length = 0;
    double pan_carry_x = 0.0, pan_carry_y = 0.0;
    uint64_t previous = SDL_GetPerformanceCounter();
    double accumulator = 0.0;
    const double tick_seconds = 1.0 / 60.0;
    uint32_t scripted_frame = 0;
    while (running) {
        if (scripted_input && scripted_frame == 0) {
            SDL_Event key = { .type = SDL_KEYDOWN };
            key.key.keysym.sym = SDLK_F2;
            SDL_PushEvent(&key);
            SDL_Event text_event = { .type = SDL_TEXTINPUT };
            snprintf(text_event.text.text, sizeof(text_event.text.text), "2718");
            SDL_PushEvent(&text_event);
            key.key.keysym.sym = SDLK_RETURN;
            SDL_PushEvent(&key);
        } else if (scripted_input && scripted_frame == 1) {
            SDL_Event key = { .type = SDL_KEYDOWN };
            key.key.keysym.sym = SDLK_r;
            SDL_PushEvent(&key);
        } else if (scripted_input && scripted_frame == 2) {
            break;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (entering_seed && event.type == SDL_TEXTINPUT) {
                for (const char *p = event.text.text; *p; ++p)
                    if (*p >= '0' && *p <= '9' && seed_length < 20u)
                        seed_text[seed_length++] = *p;
                seed_text[seed_length] = '\0';
                seed_error = false;
                continue;
            }
            if (event.type == SDL_KEYDOWN) {
                if (entering_seed) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        entering_seed = false;
                        SDL_StopTextInput();
                    } else if (event.key.keysym.sym == SDLK_BACKSPACE && seed_length) {
                        seed_text[--seed_length] = '\0';
                        seed_error = false;
                    } else if (event.key.keysym.sym == SDLK_RETURN && seed_length) {
                        errno = 0;
                        char *end = NULL;
                        unsigned long long parsed = strtoull(seed_text, &end, 10);
                        if (errno == ERANGE || !end || *end) {
                            seed_error = true;
                        } else if (!restart_demo(&view, directory, (uint64_t)parsed,
                                                 &run, zoom, overlay, spring_enabled,
                                                 marker_overlay, err, sizeof(err))) {
                            failed = true; running = false;
                        } else {
                            seed = (uint64_t)parsed;
                            entering_seed = false;
                            SDL_StopTextInput();
                            accumulator = 0.0;
                            previous = SDL_GetPerformanceCounter();
                            previous_title[0] = '\0';
                        }
                    }
                    continue;
                }
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_0: material = DC_MATERIAL_AIR; break;
                case SDLK_1: material = DC_MATERIAL_STONE; break;
                case SDLK_2: material = DC_MATERIAL_SAND; break;
                case SDLK_3: material = DC_MATERIAL_WATER; break;
                case SDLK_4: material = DC_MATERIAL_DIRT; break;
                case SDLK_5: material = DC_MATERIAL_GRAVEL; break;
                case SDLK_p: paused = !paused; break;
                case SDLK_n: single_step = true; break;
                case SDLK_m:
                    marker_overlay = !marker_overlay;
                    dc_level_view_toggle_marker_overlay(view);
                    break;
                case SDLK_F2:
                    if (!event.key.repeat) {
                        entering_seed = true;
                        seed_error = false;
                        seed_length = 0;
                        seed_text[0] = '\0';
                        SDL_StartTextInput();
                    }
                    break;
                case SDLK_r:
                    if (!event.key.repeat) {
                        if (!restart_demo(&view, directory, seed, &run, zoom,
                                          overlay, spring_enabled, marker_overlay,
                                          err, sizeof(err))) {
                            failed = true; running = false;
                        }
                        accumulator = 0.0;
                        single_step = false;
                        previous = SDL_GetPerformanceCounter();
                        previous_title[0] = '\0';
                    }
                    break;
                case SDLK_MINUS:
                    if (zoom > 1u) zoom /= 2u;
                    dc_level_view_set_zoom(view, zoom);
                    printf("Zoom %ux\n", zoom);
                    break;
                case SDLK_EQUALS: case SDLK_PLUS:
                    if (zoom < WINDOW_SCALE) zoom *= 2u;
                    dc_level_view_set_zoom(view, zoom);
                    printf("Zoom %ux\n", zoom);
                    break;
                case SDLK_v:
                    overlay = (dc_gpu_overlay_t)((overlay + 1u) % 3u);
                    dc_level_view_set_overlay(view, overlay);
                    printf("Overlay %u\n", (unsigned)overlay);
                    break;
                case SDLK_f:
                    spring_enabled = !spring_enabled;
                    dc_level_view_set_spring_enabled(view, spring_enabled);
                    printf("Spring %s\n", spring_enabled ? "on" : "off");
                    break;
                case SDLK_b: {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    uint32_t x = VIEW_WIDTH / 2, y = 4;
                    if (mx >= 0 && my >= 0)
                        dc_level_view_screen_cell(view, (uint32_t)mx,
                                                  (uint32_t)my, &x, &y);
                    if (x > VIEW_WIDTH - 4) x = VIEW_WIDTH - 4;
                    if (y > VIEW_HEIGHT - 4) y = VIEW_HEIGHT - 4;
                    if (!dc_level_view_spawn_body(view, x, y, err, sizeof(err))) {
                        failed = true; running = false;
                    }
                    break;
                }
                default: break;
                }
            }
        }
        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - previous) / (double)SDL_GetPerformanceFrequency();
        previous = now;
        if (elapsed > 0.25) elapsed = 0.25;
        if (running && !entering_seed &&
            !pan_held_keys(view, elapsed, &pan_carry_x, &pan_carry_y)) {
            snprintf(err, sizeof(err), "Cannot pan camera");
            failed = true; running = false;
        }
        if (!paused) accumulator += elapsed;
        else accumulator = 0.0;
        if (running && (single_step || (!paused && accumulator >= tick_seconds))) {
            float step_seconds = single_step ? (float)tick_seconds :
                                 (float)accumulator;
            if (step_seconds > 0.25f) step_seconds = 0.25f;
            if (!dc_level_view_step_timed(view, step_seconds, err, sizeof(err))) {
                failed = true; running = false;
            }
            single_step = false;
            accumulator = 0.0;
        }
        if (!paint_held(view, material, err, sizeof(err))) {
            failed = true; running = false;
        }
        if (running && !dc_level_view_tick(view, err, sizeof(err))) {
            failed = true; running = false;
        }
        if (running && !update_demo_title(view, seed, zoom, paused, entering_seed,
                                          seed_error, seed_text, previous_title)) {
            snprintf(err, sizeof(err), "Cannot update world status");
            failed = true; running = false;
        }
        SDL_Delay(1);
        ++scripted_frame;
    }
    if (scripted_input && !failed) {
        dc_level_view_status_t status = {0};
        if (run != 2u || seed != 2718u ||
            !dc_level_view_wait_visible(view, 5000, err, sizeof(err)) ||
            !dc_level_view_status(view, &status) ||
            status.ready_chunks != status.total_chunks) {
            snprintf(err, sizeof(err), "Scripted seed/reset controls failed");
            failed = true;
        }
    }
    if (entering_seed) SDL_StopTextInput();
    if (!dc_level_view_destroy(view, err, sizeof(err))) failed = true;
    if (failed) fprintf(stderr, "Dungeoncraft: %s\n", err);
    else if (scripted_input) printf("Interactive seed and reset controls passed\n");
    return failed ? 1 : 0;
}
