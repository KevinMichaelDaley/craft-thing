#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/renderer/gpu.h"

#include "catapult_scene.h"
#include "view_config.h"

static bool make_ground(dc_gpu_t *gpu, char *err, uint32_t cap) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    bool okay = true;
    for (uint32_t slot = 0; okay && slot < VIEW_CHUNKS_X * VIEW_CHUNKS_Y;
         slot++) {
        memset(chunk, 0, sizeof(*chunk));
        uint32_t cx = slot % VIEW_CHUNKS_X;
        uint32_t cy = slot / VIEW_CHUNKS_X;
        chunk->coord = (dc_chunk_coord_t){cx, cy};
        if (cy == 1u)
            for (uint32_t x = 0; x < DC_CHUNK_SIDE; x++)
                chunk->cells[32u * DC_CHUNK_SIDE + x].material =
                    DC_MATERIAL_STONE;
        okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, cap) &&
               dc_gpu_set_page(gpu, cx, cy, slot, err, cap);
    }
    free(chunk);
    return okay;
}

int main(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_catapult_scene_t scene = {0};
    bool okay = dc_gpu_create_window(&gpu, VIEW_WIDTH, VIEW_HEIGHT,
        VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
        "build/shaders/pattern.comp.spv", err, sizeof(err));
    if (okay) okay = dc_gpu_set_display_zoom(gpu, WINDOW_SCALE);
    if (okay) okay = make_ground(gpu, err, sizeof(err));
    if (okay) okay = dc_catapult_scene_build(gpu, true, &scene,
                                             err, sizeof(err));
    if (okay) okay = fr_renderer_enable(gpu, err, sizeof(err)) &&
                     fr_renderer_set_mode(gpu, FR_RENDERER_ALBEDO) &&
                     dc_gpu_set_tick_seconds(gpu, 1.f / 60.f) &&
                     dc_gpu_set_window_title(gpu, "Flaming catapult");
    if (okay) fprintf(stderr, "Catapult demo: 256x128 game view ready\n");
    for (uint32_t frame = 0; okay && frame < 42u; frame++) {
        uint32_t start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) goto done;
        if (frame == 8u)
            okay = dc_catapult_scene_release(gpu, &scene, err, sizeof(err));
        if (frame == 9u)
            okay = dc_catapult_scene_release_projectile(gpu, &scene,
                                                         err, sizeof(err));
        if (okay) okay = dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err));
        if (okay && frame % 10u == 0u)
            fprintf(stderr, "Catapult demo: frame %u/42\n", frame + 1u);
        uint32_t spent = SDL_GetTicks() - start;
        if (okay && spent < 16u) SDL_Delay(16u - spent);
    }
    if (okay) SDL_Delay(2000);
done:
    if (!okay) fprintf(stderr, "Catapult demo: %s\n", err);
    else fprintf(stderr, "Catapult demo: playback complete\n");
    dc_gpu_destroy(gpu);
    return okay ? 0 : 1;
}
