#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "dungeoncraft/chunk.h"
#include "level.h"

enum { VIEW_WIDTH = 256, VIEW_HEIGHT = 128, WINDOW_SCALE = 4 };

static int smoke_streamed_level(void) {
    char directory[] = "build/ui_stream_XXXXXX";
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 1; }
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    if (!view) { fprintf(stderr, "Level create: %s\n", err); return 1; }
    const char *stage = "initial load";
    bool okay = dc_level_view_wait_visible(view, 5000, err, sizeof(err));
    uint32_t color = 0;
    stage = "basin pixel";
    if (okay) okay = dc_level_view_pixel(view, 128, 40, &color, err, sizeof(err)) &&
                     color == 0xffd07030u;
    stage = "positive paint";
    if (okay) okay = dc_level_view_paint(view, 64, 5, 0, DC_MATERIAL_SAND,
                                         err, sizeof(err));
    dc_chunk_t chunk = {0};
    if (okay) okay = dc_level_view_chunk(view, (dc_chunk_coord_t){1, 0},
                                         &chunk, err, sizeof(err)) &&
                     chunk.cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND;
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
                     chunk.cells[5 * DC_CHUNK_SIDE].material == DC_MATERIAL_SAND;
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
    uint64_t seed = 314;
    if (argc == 3 && strcmp(argv[1], "--seed") == 0)
        seed = strtoull(argv[2], NULL, 10);
    char err[256] = {0};
    dc_level_view_t *view = dc_level_view_create("world_chunks", seed, err, sizeof(err));
    if (!view) { fprintf(stderr, "Level create: %s\n", err); return 1; }
    uint16_t material = DC_MATERIAL_SAND;
    bool running = true;
    bool failed = false;
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
                case SDLK_3: material = 3; break;
                case SDLK_LEFT: case SDLK_a: dc_level_view_move(view, -1, 0); break;
                case SDLK_RIGHT: case SDLK_d: dc_level_view_move(view, 1, 0); break;
                case SDLK_UP: case SDLK_w: dc_level_view_move(view, 0, -1); break;
                case SDLK_DOWN: case SDLK_s: dc_level_view_move(view, 0, 1); break;
                default: break;
                }
            }
        }
        int mx, my;
        uint32_t buttons = SDL_GetMouseState(&mx, &my);
        if (mx >= 0 && my >= 0 && mx < VIEW_WIDTH * WINDOW_SCALE &&
            my < VIEW_HEIGHT * WINDOW_SCALE) {
            if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                if (!dc_level_view_paint(view, (uint32_t)mx / WINDOW_SCALE,
                        (uint32_t)my / WINDOW_SCALE, 3, material, err, sizeof(err))) {
                    failed = true; running = false;
                }
            } else if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                if (!dc_level_view_paint(view, (uint32_t)mx / WINDOW_SCALE,
                        (uint32_t)my / WINDOW_SCALE, 3, DC_MATERIAL_AIR, err, sizeof(err))) {
                    failed = true; running = false;
                }
            }
        }
        if (running && !dc_level_view_tick(view, err, sizeof(err))) {
            failed = true; running = false;
        }
        SDL_Delay(16);
    }
    if (!dc_level_view_destroy(view, err, sizeof(err))) failed = true;
    if (failed) fprintf(stderr, "Dungeoncraft: %s\n", err);
    return failed ? 1 : 0;
}
