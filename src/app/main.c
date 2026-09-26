#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "dungeoncraft/chunk.h"
#include "level.h"

enum { VIEW_WIDTH = 256, VIEW_HEIGHT = 128, WINDOW_SCALE = 4 };

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
    if (mx < 0 || my < 0 || mx >= VIEW_WIDTH * WINDOW_SCALE ||
        my >= VIEW_HEIGHT * WINDOW_SCALE) return true;
    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))
        return dc_level_view_paint(view, (uint32_t)mx / WINDOW_SCALE,
                                   (uint32_t)my / WINDOW_SCALE, 3,
                                   material, err, cap);
    if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))
        return dc_level_view_paint(view, (uint32_t)mx / WINDOW_SCALE,
                                   (uint32_t)my / WINDOW_SCALE, 3,
                                   DC_MATERIAL_AIR, err, cap);
    return true;
}

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
    const char *rigid_stage = "rigid spawn";
    if (okay) okay = dc_level_view_spawn_body(view, 63, 2, err, sizeof(err));
    rigid_stage = "rigid cross-chunk step";
    if (okay) okay = dc_level_view_step(view, err, sizeof(err)) &&
                     dc_level_view_tick(view, err, sizeof(err));
    uint32_t rigid_color = 0;
    if (okay) okay = dc_level_view_pixel(view, 64, 2, &rigid_color, err, sizeof(err)) &&
                     rigid_color == 0xff30c040u;
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
    stage = "positive paint";
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
    stage = "negative load";
    if (okay) okay = dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    stage = "negative paint";
    if (okay) okay = dc_level_view_paint(view, 63, 5, 0, DC_MATERIAL_STONE,
                                         err, sizeof(err));
    stage = "negative eviction";
    if (okay) okay = dc_level_view_move(view, 1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    stage = "negative reload";
    if (okay) okay = dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){-1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE + 63].material == DC_MATERIAL_STONE;
    stage = "positive eviction";
    if (okay) okay = dc_level_view_move(view, 1, 0) && dc_level_view_move(view, 1, 0) &&
                     dc_level_view_move(view, 1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    uint64_t deadline = SDL_GetTicks64() + 5000;
    while (okay && dc_level_view_has_chunk(view, (dc_chunk_coord_t){1, 0}) &&
           SDL_GetTicks64() < deadline) {
        okay = dc_level_view_tick(view, err, sizeof(err));
        SDL_Delay(1);
    }
    if (okay) okay = !dc_level_view_has_chunk(view, (dc_chunk_coord_t){1, 0});
    stage = "positive reload";
    if (okay) okay = dc_level_view_move(view, -1, 0) && dc_level_view_move(view, -1, 0) &&
                     dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND &&
                     chunk.marker_count == saved_marker_count;
    bool closed = dc_level_view_destroy(view, err, sizeof(err));
    if (!okay || !closed) {
        fprintf(stderr, "Streamed level smoke failed at %s: %s\n", stage, err);
        return 1;
    }
    printf("Streamed interactive Vulkan level smoke passed\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--smoke-stream") == 0)
        return smoke_streamed_level();
    if (argc > 1 && strcmp(argv[1], "--smoke-motion") == 0)
        return smoke_moving_water();
    if (argc > 1 && strcmp(argv[1], "--smoke-motion-long") == 0)
        return smoke_moving_water_long();
    uint64_t seed = 314;
    if (argc == 3 && strcmp(argv[1], "--seed") == 0)
        seed = strtoull(argv[2], NULL, 10);
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create("world_chunks", seed, err, sizeof(err));
    if (!view) { fprintf(stderr, "Level create: %s\n", err); return 1; }
    uint16_t material = DC_MATERIAL_SAND;
    bool running = true;
    bool failed = false;
    bool paused = false;
    bool single_step = false;
    uint64_t previous = SDL_GetPerformanceCounter();
    double accumulator = 0.0;
    const double tick_seconds = 1.0 / 60.0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_0: material = DC_MATERIAL_AIR; break;
                case SDLK_1: material = DC_MATERIAL_STONE; break;
                case SDLK_2: material = DC_MATERIAL_SAND; break;
                case SDLK_3: material = DC_MATERIAL_WATER; break;
                case SDLK_p: paused = !paused; break;
                case SDLK_n: single_step = true; break;
                case SDLK_m: dc_level_view_toggle_marker_overlay(view); break;
                case SDLK_b: {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    uint32_t x = mx >= 0 && mx < VIEW_WIDTH * WINDOW_SCALE ?
                        (uint32_t)mx / WINDOW_SCALE : VIEW_WIDTH / 2;
                    uint32_t y = my >= 0 && my < VIEW_HEIGHT * WINDOW_SCALE ?
                        (uint32_t)my / WINDOW_SCALE : 4;
                    if (x > VIEW_WIDTH - 4) x = VIEW_WIDTH - 4;
                    if (y > VIEW_HEIGHT - 4) y = VIEW_HEIGHT - 4;
                    if (!dc_level_view_spawn_body(view, x, y, err, sizeof(err))) {
                        failed = true; running = false;
                    }
                    break;
                }
                case SDLK_LEFT: case SDLK_a: dc_level_view_move(view, -1, 0); break;
                case SDLK_RIGHT: case SDLK_d: dc_level_view_move(view, 1, 0); break;
                case SDLK_UP: case SDLK_w: dc_level_view_move(view, 0, -1); break;
                case SDLK_DOWN: case SDLK_s: dc_level_view_move(view, 0, 1); break;
                default: break;
                }
            }
        }
        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - previous) / (double)SDL_GetPerformanceFrequency();
        previous = now;
        if (elapsed > 0.25) elapsed = 0.25;
        if (!paused) accumulator += elapsed;
        else accumulator = 0.0;
        uint32_t steps = 0;
        while (running && (single_step || (!paused && accumulator >= tick_seconds)) && steps < 4) {
            if (!dc_level_view_step(view, err, sizeof(err))) {
                failed = true; running = false;
                break;
            }
            single_step = false;
            if (accumulator >= tick_seconds) accumulator -= tick_seconds;
            ++steps;
        }
        if (accumulator > 4.0 * tick_seconds) accumulator = 4.0 * tick_seconds;
        if (!paint_held(view, material, err, sizeof(err))) {
            failed = true; running = false;
        }
        if (running && !dc_level_view_tick(view, err, sizeof(err))) {
            failed = true; running = false;
        }
        SDL_Delay(1);
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) failed = true;
    if (failed) fprintf(stderr, "Dungeoncraft: %s\n", err);
    return failed ? 1 : 0;
}
