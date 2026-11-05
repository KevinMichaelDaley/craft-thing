#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/coating_layout.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/renderer/gpu.h"
#include "ferrum/structural/gpu.h"

#include "catapult_scene.h"
#include "view_config.h"

enum {
    GREEK_DEMO_FRAMES = 1800u,
    GREEK_DEMO_RELEASE_FRAME = 8u,
    GREEK_DEMO_POUCH_FRAME = 11u,
    /* Coin row on the battlement deck wood, above the wall face. */
    GREEK_COIN_X0 = 200u,
    GREEK_COIN_SPACING = 6u,
    GREEK_COIN_Y = 94u
};

static uint32_t frame_pixels[VIEW_WIDTH * VIEW_HEIGHT];

static bool save_frame(dc_gpu_t *gpu, const char *path, char *err,
                       uint32_t cap) {
    bool okay = dc_gpu_readback(gpu, frame_pixels,
                                VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    /* Display gamma: the night-launch scene reads near-black raw; lift it
     * so the captured video shows the terrain, load and flame. */
    for (uint32_t i = 0u; i < VIEW_WIDTH * VIEW_HEIGHT; i++) {
        uint32_t p = frame_pixels[i];
        uint32_t r = (p & 255u) * 4u, g = ((p >> 8u) & 255u) * 4u,
                 b = ((p >> 16u) & 255u) * 4u;
        frame_pixels[i] = (r > 255u ? 255u : r) |
                          ((g > 255u ? 255u : g) << 8u) |
                          ((b > 255u ? 255u : b) << 16u);
    }
    if (okay) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            frame_pixels, VIEW_WIDTH, VIEW_HEIGHT, 32, VIEW_WIDTH * 4u,
            SDL_PIXELFORMAT_ABGR8888);
        okay = surface && SDL_SaveBMP(surface, path) == 0;
        SDL_FreeSurface(surface);
    }
    return okay;
}

/* Strike the projectile's coal into a lit greek compound: film stock and
 * the burning flag land on every parcel, with the strike heat so the
 * reaction is viable from the first tick. */
static bool greek_coat_projectile(dc_gpu_t *gpu, uint32_t body, char *err,
                                  uint32_t cap) {
    for (uint32_t py = 0u; py < 9u; py++)
        for (uint32_t px = 0u; px < 9u; px++) {
            fr_reactive_record_t parcel;
            bool found = false;
            if (!fr_gpu_reactive_read_body(gpu, body, px, py, &parcel, &found,
                                           err, cap))
                return false;
            if (!found) continue;
            parcel.state.coating_oil_fuel_fp = 16384u;
            parcel.state.coating_jelly_fuel_fp = 16384u;
            parcel.state.coating_oxidizer_fp = 16384u;
            parcel.state.coating_threshold = .5f;
            parcel.state.coating_integral = .6f;
            parcel.state.flags |= FR_REACTIVE_COATING_FLAMING;
            double hot = fr_reactive_state_energy_at_temperature(
                &parcel.state, 700.f);
            double cold = fr_reactive_state_energy(&parcel.state);
            if (hot > cold)
                parcel.state.thermal_energy = (float)hot;
            if (!fr_gpu_reactive_seed_body(gpu, body, px, py, &parcel.state,
                                           err, cap))
                return false;
        }
    return true;
}

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

/* Coins: gold and ingredient cells resting on the battlement deck wood,
 * seeded as reactive records so the brands can coat and corrode them. */
static bool stage_deck_coins(dc_gpu_t *gpu, char *err, uint32_t cap) {
    fr_reactive_state_t gold, ingredient;
    if (!fr_reactive_state_default(DC_MATERIAL_GOLD, 65536u, 0u, 293.15f,
                                   0x6E01u, &gold, err, cap))
        return false;
    if (!fr_reactive_state_default(DC_MATERIAL_MAGIC_INGREDIENT, 65536u, 0u,
                                   293.15f, 0x6E02u, &ingredient, err, cap))
        return false;
    for (uint32_t which = 0u; which < 2u; which++) {
        uint32_t material = which == 0u ? DC_MATERIAL_GOLD
                                        : DC_MATERIAL_MAGIC_INGREDIENT;
        uint32_t y = 91u;
        for (uint32_t k = 0u; k < 4u; k++) {
            uint32_t x = 177u;
            if (k == 1u) y = 93u;
            else if (k == 2u) {y = 95u;x = 179u;}
            else if (k == 3u) y = 90u;
            /* The coin materializes its own cell on the deck, then binds
             * its reactive record. */
            if (!dc_gpu_paint_material(gpu, x, y, 0u, material, err, cap))
                return false;
            fr_reactive_state_t state = which == 0u ? gold : ingredient;
            /* Parcel identity above every staged origin, in the same high
             * space the forest fixture reserves for runtime parcels. */
            uint64_t parcel = (6ull << 58) + 0x6E000u + which * 16ull + k;
            state.parcel_id_low = (uint32_t)parcel;
            state.parcel_id_high = (uint32_t)(parcel >> 32);
            state.origin_id_low = state.parcel_id_low;
            state.origin_id_high = state.parcel_id_high;
            uint32_t slot = (y / 64u) * VIEW_CHUNKS_X + x / 64u;
            if (!fr_gpu_reactive_seed_cell(gpu, slot, (y % 64u) * 64u + x % 64u,
                                           &state, err, cap))
                return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_catapult_scene_t scene = {0};
    mkdir("build/screenshots", 0777);
    bool capture_all = false, headless = false, wall_cleared = false;
    uint32_t total_frames = GREEK_DEMO_FRAMES;
    char capture_dir[160] = "build/screenshots/greek_demo_frames";
    uint32_t captured_frames = 0u;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--capture") == 0) {
            capture_all = true;
            mkdir(capture_dir, 0777);
        } else if (strcmp(argv[i], "--headless") == 0)
            headless = true;
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            total_frames = (uint32_t)strtoul(argv[++i], NULL, 10);
    }
    bool okay = headless
        ? dc_gpu_create(&gpu, VIEW_WIDTH, VIEW_HEIGHT,
                        "build/shaders/pattern.comp.spv", err, sizeof(err))
        : dc_gpu_create_window(&gpu, VIEW_WIDTH, VIEW_HEIGHT,
            VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
            "build/shaders/pattern.comp.spv", err, sizeof(err)) != 0;
    if (okay && !headless) okay = dc_gpu_set_display_zoom(gpu, WINDOW_SCALE);
    if (okay) okay = make_ground(gpu, err, sizeof(err)) &&
                     dc_catapult_scene_build_with_chunk_stride(gpu, true,
                         VIEW_CHUNKS_X, &scene, err, sizeof(err));
    fr_renderer_sun_t sun = {.direction = {-.35f, -.75f, .55f},
        .intensity = 3.6f, .color = {1.f, .97f, .9f},
        .known_sky = FR_SKY_TOP | FR_SKY_LEFT};
    if (okay) okay = fr_renderer_enable(gpu, err, sizeof(err));
    if (okay) okay = fr_renderer_set_sources(gpu, NULL, 0, sun);
    if (okay) okay = fr_renderer_set_mode(gpu, FR_RENDERER_LIT);
    if (okay) okay = dc_gpu_set_tick_seconds(gpu, 1.f / 60.f);
    if (okay && !headless) okay = dc_gpu_set_window_title(gpu,
        "Greek fire: burning load vs the battlement");
    /* Terrain albedo: stone ground, wood battlement, gold coins — the
     * scene stages no colors of its own. */
    for (uint32_t slot = 0u; okay && slot < VIEW_CHUNKS_X * VIEW_CHUNKS_Y;
         slot++) {
        dc_chunk_colors_t *colors = calloc(1, sizeof(*colors));
        if (!colors) { okay = false; break; }
        uint32_t cx = slot % VIEW_CHUNKS_X, cy = slot / VIEW_CHUNKS_X;
        for (uint32_t i = 0u; i < 4096u; i++) {
            uint32_t x = cx * 64u + i % 64u, y = cy * 64u + i / 64u;
            float *c = colors->terrain[i];
            c[3] = 1.f;
            uint32_t grain = (x * 3u + y * 11u) & 15u;
            if (y >= 87u && y <= 97u && x >= 196u && x <= 242u) {
                /* Battlement deck wood. */
                c[0] = .30f + grain / 255.f;
                c[1] = .17f + grain / 400.f;
                c[2] = .06f;
            } else if (y >= 96u) {
                /* Ground stone. */
                c[0] = .32f + grain / 255.f;
                c[1] = .31f + grain / 300.f;
                c[2] = .30f;
            } else {
                c[0] = .07f;c[1] = .08f;c[2] = .10f;
            }
        }
        for (uint32_t k = 0u; k < 4u; k++)
            for (uint32_t which = 0u; which < 2u; which++) {
                uint32_t x = 200u + k * 6u + which * 2u;
                uint32_t y = 94u + which;
                uint32_t i = (y % 64u) * 64u + x % 64u;
                uint32_t slot_of_coin = (y / 64u) * VIEW_CHUNKS_X + x / 64u;
                if (slot_of_coin != slot) continue;
                colors->terrain[i][0] = .85f;
                colors->terrain[i][1] = .66f;
                colors->terrain[i][2] = which == 0u ? .22f : .45f;
            }
        okay = fr_renderer_upload_chunk_colors(gpu, slot, colors, err,
                                               sizeof(err));
        free(colors);
    }
    if (okay) okay = scene.coal_body != 0u &&
                     greek_coat_projectile(gpu, scene.coal_body, err,
                                           sizeof(err)) &&
                     stage_deck_coins(gpu, err, sizeof(err));
    if (okay) fprintf(stderr, "Greek catapult demo: %dx%d view ready, coins "
                      "on the battlement deck\n", VIEW_WIDTH, VIEW_HEIGHT);
    for (uint32_t frame = 0u; okay && frame < total_frames; frame++) {
        uint32_t start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) goto done;
        if (frame == GREEK_DEMO_RELEASE_FRAME)
            okay = dc_catapult_scene_release(gpu, &scene, err, sizeof(err));
        if (frame == GREEK_DEMO_POUCH_FRAME)
            okay = dc_catapult_scene_release_projectile(gpu, &scene,
                                                        err, sizeof(err));
        if (okay && headless)
            okay = dc_gpu_tick_step(gpu, err, sizeof(err));
        else if (okay)
            okay = dc_gpu_present_chunks_steps(gpu, 1u, err, sizeof(err));
        if (okay && frame == GREEK_DEMO_POUCH_FRAME + 2u) {
            dc_gpu_world_body_t ball;
            if (dc_gpu_read_world_body(gpu, scene.coal_body, &ball, err,
                                       sizeof(err)))
                fprintf(stderr,
                        "greek launch: pos %.1f %.1f vel %.2f %.2f c/t\n",
                        (float)ball.chunk.x * 64.f +
                            ball.body.x_fp / 65536.f,
                        (float)ball.chunk.y * 64.f +
                            ball.body.y_fp / 65536.f,
                        ball.body.vx_fp / 65536.f,
                        ball.body.vy_fp / 65536.f);
        }
        if (okay && frame > GREEK_DEMO_POUCH_FRAME + 2u && !wall_cleared) {
            dc_gpu_world_body_t ball;
            if (dc_gpu_read_world_body(gpu, scene.coal_body, &ball, err,
                                       sizeof(err))) {
                float bx = (float)ball.chunk.x * 64.f +
                           ball.body.x_fp / 65536.f;
                float by = (float)ball.chunk.y * 64.f +
                           ball.body.y_fp / 65536.f;
                if (bx > 150.f && by < 150.f) wall_cleared = true;
                else if (frame % 20u == 0u)
                    fprintf(stderr,
                            "greek flight: frame %u ball %.1f %.1f\n", frame,
                            bx, by);
            }
        }
        if (okay && capture_all) {
            char path[256];
            snprintf(path, sizeof(path), "%s/frame_%05u.bmp", capture_dir,
                     captured_frames);
            okay = save_frame(gpu, path, err, sizeof(err));
            if (okay) captured_frames++;
        }
        if (okay && frame % 120u == 30u) {
            for (uint32_t k = 0u; k < 2u; k++) {
                uint32_t x = 200u + k * 6u;
                uint32_t y = 94u;
                uint32_t slot = (y / 64u) * VIEW_CHUNKS_X + x / 64u;
                fr_reactive_record_t record;
                bool found = false;
                if (fr_gpu_reactive_read_cell(gpu, slot,
                                              (y % 64u) * 64u + x % 64u,
                                              &record, &found, err,
                                              sizeof(err)) &&
                    found)
                    fprintf(stderr,
                            "Greek demo %us: %s coin corroded %.4f "
                            "(film %u/%u/%u)\n",
                            frame / 60u,
                            k == 0u ? "gold" : "ingredient",
                            record.state.corroded_fraction,
                            record.state.coating_oil_fuel_fp,
                            record.state.coating_jelly_fuel_fp,
                            record.state.coating_oxidizer_fp);
            }
        }
        if (okay && frame == 330u) {
            dc_gpu_world_body_t ball;
            if (dc_gpu_read_world_body(gpu, scene.coal_body, &ball, err,
                                       sizeof(err)))
                fprintf(stderr,
                        "greek rest: pos %.1f %.1f vel %.2f %.2f\n",
                        (float)ball.chunk.x * 64.f +
                            ball.body.x_fp / 65536.f,
                        (float)ball.chunk.y * 64.f +
                            ball.body.y_fp / 65536.f,
                        ball.body.vx_fp / 65536.f,
                        ball.body.vy_fp / 65536.f);
        }
        if (okay && frame % 120u == 60u) {
            static const uint32_t coin_cells[4][2] = {
                {177u, 91u}, {177u, 93u}, {179u, 95u}, {177u, 90u}};
            static const char *coin_names[4] = {"gold", "ingredient", "gold",
                                                "ingredient"};
            for (uint32_t which = 0u; which < 4u; which++) {
                uint32_t cx = coin_cells[which][0], cy = coin_cells[which][1];
                uint32_t slot = (cy / 64u) * VIEW_CHUNKS_X + cx / 64u;
                fr_reactive_record_t record;
                bool found = false;
                bool read_ok = fr_gpu_reactive_read_cell(
                    gpu, slot, (cy % 64u) * 64u + cx % 64u, &record, &found,
                    err, sizeof(err));
                fprintf(stderr, "greek coin %s at %u,%u: read %d found %d "
                                "film %u/%u/%u corroded %.4f\n",
                        coin_names[which], cx, cy, read_ok, found,
                        found ? record.state.coating_oil_fuel_fp : 0u,
                        found ? record.state.coating_jelly_fuel_fp : 0u,
                        found ? record.state.coating_oxidizer_fp : 0u,
                        found ? record.state.corroded_fraction : 0.f);
            }
        }
        if (okay && frame % 120u == 0u)
            fprintf(stderr, "Greek catapult demo: frame %u/%u\n", frame + 1u,
                    total_frames);
        uint32_t spent = SDL_GetTicks() - start;
        if (okay && spent < 16u) SDL_Delay(16u - spent);
    }
done:
    if (!okay) fprintf(stderr, "Greek catapult demo: %s\n", err);
    else fprintf(stderr, "Greek catapult demo: playback complete\n");
    dc_gpu_destroy(gpu);
    return okay ? 0 : 1;
}
