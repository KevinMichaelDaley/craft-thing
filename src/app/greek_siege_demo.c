#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/renderer/gpu.h"

#include "../../src/vulkan/gpu_internal.h"

#include "siege_scene.h"
#include "view_config.h"

enum {
    GREEK_DEMO_FRAMES = 1800u,
    GREEK_DEMO_RELEASE_FRAME = 8u,
    GREEK_DEMO_FUSE_FRAME = 11u
};

static uint32_t frame_pixels[640u * 448u];

static bool save_frame(dc_gpu_t *gpu, const char *path, char *err,
                       uint32_t cap) {
    bool okay = dc_gpu_readback(gpu, frame_pixels,
                                VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    if (okay) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            frame_pixels, VIEW_WIDTH, VIEW_HEIGHT, 32, VIEW_WIDTH * 4u,
            SDL_PIXELFORMAT_ABGR8888);
        okay = surface && SDL_SaveBMP(surface, path) == 0;
        SDL_FreeSurface(surface);
    }
    return okay;
}

/* Coin telemetry: film stock and corrosion fraction of the battlement row. */
static void coin_report(dc_gpu_t *gpu, char *err, uint32_t cap) {
    static const uint32_t coins[4] = {154u, 157u, 160u, 161u};
    for (unsigned which = 0u; which < 4u; which++) {
        fr_reactive_record_t record;
        bool found = false;
        uint32_t slot = (149u / 64u) * (gpu->page_width) + coins[which] / 64u;
        if (!fr_gpu_reactive_read_cell(gpu, slot - 1u,
                                       (149u % 64u) * 64u + coins[which] % 64u,
                                       &record, &found, err, cap))
            return;
        if (found)
            fprintf(stderr,
                    "coin %u: film %u/%u/%u corroded %.4f flags %x\n",
                    coins[which], record.state.coating_oil_fuel_fp,
                    record.state.coating_jelly_fuel_fp,
                    record.state.coating_oxidizer_fp,
                    record.state.corroded_fraction, record.state.flags);
    }
}

int main(int argc, char **argv) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_siege_scene_t scene = {0};
    mkdir("build/screenshots", 0777);
    bool capture_all = false;
    char capture_dir[160] = "build/screenshots/greek_demo_frames";
    uint32_t captured_frames = 0u;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--capture") == 0) {
            capture_all = true;
            mkdir(capture_dir, 0777);
        }
    bool okay = dc_gpu_create_window(&gpu, SIM_WIDTH, SIM_HEIGHT,
        VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
        "build/shaders/pattern.comp.spv", err, sizeof(err)) != 0;
    if (okay) okay = dc_gpu_set_viewport(gpu, 0u, 0u, VIEW_WIDTH,
                                         VIEW_HEIGHT) &&
                     dc_gpu_set_display_zoom(gpu, WINDOW_SCALE) &&
                     dc_siege_scene_build_load(
                         gpu, false, DC_SIEGE_LOAD_GREEK, &scene,
                         err, sizeof(err));
    fr_renderer_sun_t sun = {.direction = {-.4f, -.8f, .45f},
        .intensity = 1.8f, .color = {1.f, .95f, .85f},
        .known_sky = FR_SKY_TOP | FR_SKY_LEFT};
    if (okay) okay = fr_renderer_enable(gpu, err, sizeof(err)) &&
                     fr_renderer_set_sources(gpu, NULL, 0, sun) &&
                     fr_renderer_set_mode(gpu, FR_RENDERER_LIT) &&
                     dc_gpu_set_tick_seconds(gpu, 1.f / 60.f) &&
                     dc_gpu_set_window_title(gpu,
                         "Greek fire siege: burning load vs the battlement");
    if (okay) fprintf(stderr, "Greek siege demo: %ux%u view ready, coins on "
                      "the battlement\n", VIEW_WIDTH, VIEW_HEIGHT);
    for (uint32_t frame = 0u; okay && frame < GREEK_DEMO_FRAMES; frame++) {
        uint32_t start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) goto done;
        /* Prompt launch: release, then the struck fuse flares as the load
         * clears the pouch. */
        if (frame == GREEK_DEMO_RELEASE_FRAME)
            okay = dc_siege_scene_release(gpu, &scene, err, sizeof(err));
        if (frame == GREEK_DEMO_FUSE_FRAME)
            okay = dc_siege_scene_ignite_load(gpu, &scene, 0.6f, err,
                                              sizeof(err));
        if (okay) okay = dc_gpu_present_chunks_steps(gpu, 1u, err, sizeof(err));
        if (okay && capture_all) {
            char path[256];
            snprintf(path, sizeof(path), "%s/frame_%05u.bmp", capture_dir,
                     captured_frames);
            okay = save_frame(gpu, path, err, sizeof(err));
            if (okay) captured_frames++;
        }
        if (okay && frame % 60u == 30u) {
            coin_report(gpu, err, sizeof(err));
        }
        if (okay && (frame == 8u || frame == 14u || frame == 60u ||
                     frame == 300u || frame == GREEK_DEMO_FRAMES - 1u)) {
            char path[160];
            snprintf(path, sizeof(path),
                     "build/screenshots/greek_siege_f%04u.bmp", frame);
            if (!save_frame(gpu, path, err, sizeof(err)))
                fprintf(stderr, "Greek demo: capture %s failed: %s\\n", path,
                        err);
        }
        if (okay && frame % 120u == 0u)
            fprintf(stderr, "Greek siege demo: frame %u/%u\\n", frame + 1u,
                    GREEK_DEMO_FRAMES);
        uint32_t spent = SDL_GetTicks() - start;
        if (okay && spent < 16u) SDL_Delay(16u - spent);
    }
done:
    if (!okay) fprintf(stderr, "Greek siege demo: %s\n", err);
    else fprintf(stderr, "Greek siege demo: playback complete\n");
    dc_gpu_destroy(gpu);
    return okay ? 0 : 1;
}
