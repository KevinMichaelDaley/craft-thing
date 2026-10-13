#define main pixel_component_fixture_main
#include "../vulkan/pixel_component_tests.c"
#undef main

#include <SDL.h>
#include <string.h>
#include <sys/stat.h>

enum { PIXEL_VIS_WIDTH = 480, PIXEL_VIS_HEIGHT = 270, PIXEL_VIS_TICKS = 180 };
static bool pixel_snapshot(dc_gpu_t *gpu, const char *path, uint32_t *pixels,
                            char *err) {
    if (!dc_gpu_readback(gpu, pixels, PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT, err, 256))
        return false;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, PIXEL_VIS_WIDTH, PIXEL_VIS_HEIGHT, 32,
        PIXEL_VIS_WIDTH * 4, SDL_PIXELFORMAT_ABGR8888);
    bool okay = surface && SDL_SaveBMP(surface, path) == 0;
    if (surface)
        SDL_FreeSurface(surface);
    return okay;
}
static void test_two_automatic_concave_pixel_bodies_rotate_collide_and_pause_stably(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create_window(&gpu, PIXEL_VIS_WIDTH, PIXEL_VIS_HEIGHT,
                                     1920, 1080, "build/shaders/pattern.comp.spv",
                                     err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned cy = 0; cy < 5; cy++)
        for (unsigned cx = 0; cx < 8; cx++) {
            memset(chunk, 0, sizeof(*chunk));
            chunk->coord = (dc_chunk_coord_t){cx, cy};
            for (unsigned y = 0; y < 64; y++)
                for (unsigned x = 0; x < 64; x++) {
                    unsigned wx = cx * 64 + x, wy = cy * 64 + y;
                    unsigned material = DC_MATERIAL_AIR;
                    if (wy == 244 && wx < PIXEL_VIS_WIDTH)
                        material = DC_MATERIAL_STONE;
                    else if (wx >= 180 && wx < 260 && wy >= 30 && wy < 94 &&
                             (wx < 186 || wx >= 254 || wy >= 88))
                        material = DC_MATERIAL_WOOD;
                    else if (wx >= 198 && wx < 270 && wy >= 115 && wy < 163 &&
                             (wx < 204 || (wy - 115) % 6 < 3))
                        material = wx < 210 ? DC_MATERIAL_STONE : DC_MATERIAL_DENSE_WOOD;
                    chunk->cells[y * 64 + x].material = material;
                }
            unsigned slot = cy * 8 + cx;
            ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)));
            ASSERT_TRUE(dc_gpu_set_page(gpu, cx, cy, slot, err, sizeof(err)));
        }
    free(chunk);
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_set_fluid_interval(gpu, 2));
    ASSERT_TRUE(dc_gpu_set_viewport(gpu, 0, 0, PIXEL_VIS_WIDTH, PIXEL_VIS_HEIGHT));
    ASSERT_TRUE(dc_gpu_set_display_zoom(gpu, 4));
    ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_RIGID_IDS));
    ASSERT_TRUE(dc_gpu_set_window_title(gpu,
        "Automatic pixel bodies: hollow wood U and mixed-material branching body"));
    ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    uint32_t *pixels = malloc(PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT * sizeof(*pixels));
    uint32_t *paused = malloc(PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT * sizeof(*paused));
    dc_gpu_contact_t *contacts = malloc(8192 * sizeof(*contacts));
    ASSERT_TRUE(pixels && paused && contacts);
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(pixel_snapshot(gpu, "build/screenshots/pixel_concave_before.bmp", pixels, err));
    uint32_t count;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, 0x80000001u, NULL, 0, &count, err, sizeof(err)));
    ASSERT_EQ(count, 1176u);
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, 0x80000002u, NULL, 0, &count, err, sizeof(err)));
    ASSERT_EQ(count, 1872u);
    ASSERT_EQ(pixels[40 * PIXEL_VIS_WIDTH + 182], 0xff30c040u);
    ASSERT_EQ(pixels[116 * PIXEL_VIS_WIDTH + 230], 0xffe09040u);
    ASSERT_TRUE(pixels[50 * PIXEL_VIS_WIDTH + 220] != 0xff30c040u);
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu, 0x80000001u, 0, .005f, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu, 0x80000002u, 0, -.003f, 0, err, sizeof(err)));
    SDL_Delay(750);
    bool body_contact = false, terrain_contact = false, rotated = false;
    uint64_t began = SDL_GetTicks64();
    for (unsigned tick = 0; tick < PIXEL_VIS_TICKS; tick++) {
        uint64_t started = SDL_GetTicks64();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            ASSERT_TRUE(event.type != SDL_QUIT);
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
        dc_gpu_contact_stats_t stats;
        ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 8192, err, sizeof(err)));
        ASSERT_EQ(stats.overflow, 0u);
        for (unsigned c = 0; c < stats.count; c++) {
            if (contacts[c].kind == DC_GPU_CONTACT_BODY && !body_contact) {
                ASSERT_TRUE(pixel_snapshot(gpu, "build/screenshots/pixel_concave_impact.bmp", pixels, err));
                body_contact = true;
            }
            terrain_contact |= contacts[c].kind == DC_GPU_CONTACT_TERRAIN;
        }
        dc_gpu_body_motion_t a, b;
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 0x80000001u, &a, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 0x80000002u, &b, err, sizeof(err)));
        rotated |= fabs(a.angle) > .01 || fabs(b.angle) > .01;
        ASSERT_TRUE(fabs(a.mass - 705.6) < .002);
        ASSERT_TRUE(fabs(b.mass - 2894.4) < .003);
        uint64_t elapsed = SDL_GetTicks64() - started;
        if (elapsed < 17)
            SDL_Delay(17 - elapsed);
    }
    ASSERT_TRUE(body_contact && terrain_contact && rotated);
    ASSERT_TRUE(pixel_snapshot(gpu, "build/screenshots/pixel_concave_after.bmp", pixels, err));
    memcpy(paused, pixels, PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT * sizeof(*pixels));
    for (unsigned frame = 0; frame < 4; frame++) {
        ASSERT_TRUE(dc_gpu_present_chunks(gpu, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_readback(gpu, pixels, PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT, err, sizeof(err)));
        ASSERT_EQ(memcmp(paused, pixels, PIXEL_VIS_WIDTH * PIXEL_VIS_HEIGHT * sizeof(*pixels)), 0);
    }
    ASSERT_TRUE(dc_gpu_set_overlay(gpu, DC_GPU_OVERLAY_STRUCTURAL_STRESS));
    ASSERT_TRUE(dc_gpu_present_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(pixel_snapshot(gpu, "build/screenshots/pixel_concave_materials.bmp", pixels, err));
    printf("Quarter-native 480x270: %u physical ticks in %.3fs, body/terrain contacts, exact mass and four stable paused frames\n",
           PIXEL_VIS_TICKS, (SDL_GetTicks64() - began) / 1000.);
    SDL_Delay(750);
    free(contacts);
    free(paused);
    free(pixels);
    dc_gpu_destroy(gpu);
    PASS();
}
int main(void) {
    RUN(test_two_automatic_concave_pixel_bodies_rotate_collide_and_pause_stably);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
