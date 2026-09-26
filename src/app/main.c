#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "dungeoncraft/gpu.h"

enum { GRID_WIDTH = 256, GRID_HEIGHT = 144, WINDOW_SCALE = 4 };

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "--smoke") == 0;
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create_window(&gpu, GRID_WIDTH, GRID_HEIGHT,
            GRID_WIDTH * WINDOW_SCALE, GRID_HEIGHT * WINDOW_SCALE,
            "build/shaders/pattern.comp.spv", err, sizeof(err))) {
        fprintf(stderr, "GPU window: %s\n", err);
        return 1;
    }
    if (!dc_gpu_pattern(gpu, err, sizeof(err))) {
        fprintf(stderr, "GPU pattern: %s\n", err);
        dc_gpu_destroy(gpu);
        return 1;
    }
    uint32_t color = 0xff3030ffu;
    bool running = true;
    unsigned frames = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_1: color = 0xff3030ffu; break;
                case SDLK_2: color = 0xff30ff30u; break;
                case SDLK_3: color = 0xffff8030u; break;
                case SDLK_c:
                    if (!dc_gpu_pattern(gpu, err, sizeof(err))) running = false;
                    break;
                default: break;
                }
            }
        }
        if (smoke && frames == 0) {
            if (!dc_gpu_paint(gpu, 32, 32, 3, color, err, sizeof(err))) running = false;
            uint32_t cells[GRID_WIDTH * GRID_HEIGHT];
            if (!dc_gpu_readback(gpu, cells, GRID_WIDTH * GRID_HEIGHT, err, sizeof(err)) ||
                cells[32 * GRID_WIDTH + 32] != color) {
                fprintf(stderr, "Interactive brush readback failed: %s\n", err);
                running = false;
            }
        } else if (!smoke) {
            int mx, my;
            uint32_t buttons = SDL_GetMouseState(&mx, &my);
            if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                if (!dc_gpu_paint(gpu, (uint32_t)(mx / WINDOW_SCALE),
                        (uint32_t)(my / WINDOW_SCALE), 3, color, err, sizeof(err)))
                    running = false;
            }
        }
        if (running && !dc_gpu_present(gpu, err, sizeof(err))) {
            fprintf(stderr, "GPU present: %s\n", err);
            running = false;
        }
        if (smoke && ++frames == 2) break;
        SDL_Delay(16);
    }
    dc_gpu_destroy(gpu);
    if (err[0]) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    if (smoke) printf("Interactive Vulkan smoke passed\n");
    return 0;
}
