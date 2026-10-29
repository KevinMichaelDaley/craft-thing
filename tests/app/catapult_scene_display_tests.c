#include <SDL.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/renderer/gpu.h"
#include "../../src/app/view_config.h"

static bool catapult_window(dc_gpu_t **gpu, uint32_t width, uint32_t height,
                            const char *shader, char *err, uint32_t cap) {
    return dc_gpu_create_window(gpu, width, height,
                                VIEW_WIDTH * WINDOW_SCALE,
                                VIEW_HEIGHT * WINDOW_SCALE,
                                shader, err, cap) &&
           dc_gpu_set_display_zoom(*gpu, WINDOW_SCALE);
}

#define dc_gpu_create catapult_window
#define DC_CATAPULT_DISPLAY_INCLUDE
#include "catapult_scene_tests.c"
#undef dc_gpu_create

static bool capture(dc_gpu_t *gpu, const char *path, char *err,
                    uint32_t cap) {
    if (!dc_gpu_present_chunks_steps(gpu, 0, err, cap)) return false;
    uint32_t *pixels = calloc(VIEW_WIDTH * VIEW_HEIGHT, sizeof(*pixels));
    if (!pixels) return false;
    bool okay = dc_gpu_readback(gpu, pixels, VIEW_WIDTH * VIEW_HEIGHT, err, cap);
    SDL_Surface *source = okay ? SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, VIEW_WIDTH, VIEW_HEIGHT, 32, VIEW_WIDTH * 4,
        SDL_PIXELFORMAT_ABGR8888) : NULL;
    SDL_Surface *image = source ? SDL_CreateRGBSurfaceWithFormat(
        0, VIEW_WIDTH * WINDOW_SCALE, VIEW_HEIGHT * WINDOW_SCALE,
        32, SDL_PIXELFORMAT_ABGR8888) : NULL;
    okay = image && SDL_BlitScaled(source, NULL, image, NULL) == 0 &&
           SDL_SaveBMP(image, path) == 0;
    if (image) SDL_FreeSurface(image);
    if (source) SDL_FreeSurface(source);
    free(pixels);
    return okay;
}

static void test_render_moving_arm_without_fire_occlusion(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = catapult_grid();
    ASSERT_TRUE(gpu);
    dc_catapult_scene_t scene = {0};
    bool built = dc_catapult_scene_build(gpu, false, &scene, err, sizeof(err));
    if (!built) fprintf(stderr, "cold display scene: %s\n", err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(fr_renderer_enable(gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(gpu, FR_RENDERER_ALBEDO));
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_loaded.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 20; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_cocked.bmp",
                        err, sizeof(err)));
    uint32_t pixels[VIEW_WIDTH * VIEW_HEIGHT];
    ASSERT_TRUE(dc_gpu_readback(gpu,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,sizeof(err)));
    uint32_t air = pixels[40u * VIEW_WIDTH + 100u];
    ASSERT_TRUE(((air >> 8u) & 255u) < 40u &&
                ((air >> 16u) & 255u) < 40u);
    uint32_t cocked_wood=0;
    for(uint32_t y=60;y<80;y++) for(uint32_t x=60;x<85;x++)
        if((pixels[y*VIEW_WIDTH+x]&255u)>70u) cocked_wood++;
    ASSERT_TRUE(cocked_wood>10u);
    dc_gpu_body_motion_t held,launched;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,scene.arm_body,&held,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(gpu, &scene, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,scene.arm_body,&launched,
        err,sizeof(err)));
    ASSERT_TRUE(launched.angle>held.angle+.04f);
    ASSERT_TRUE(dc_catapult_scene_release_projectile(gpu,&scene,err,sizeof(err)));
    for (uint32_t tick = 0; tick < 8; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_flight.bmp",
                        err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,sizeof(err)));
    uint32_t flight_wood=0;
    for(uint32_t y=60;y<80;y++) for(uint32_t x=60;x<85;x++)
        if((pixels[y*VIEW_WIDTH+x]&255u)>70u) flight_wood++;
    ASSERT_TRUE(flight_wood>10u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_render_real_loaded_flight_and_impact(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = catapult_grid();
    ASSERT_TRUE(gpu);
    dc_catapult_scene_t scene = {0};
    bool built = dc_catapult_scene_build(gpu, true, &scene, err, sizeof(err));
    if (!built) fprintf(stderr, "hot display scene: %s\n", err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(fr_renderer_enable(gpu, err, sizeof(err)));
    fr_renderer_sun_t sun = {.direction = {-.4f, -.8f, .45f},
        .intensity = 1.8f, .color = {1.f, .95f, .85f},
        .known_sky = FR_SKY_TOP | FR_SKY_LEFT};
    ASSERT_TRUE(fr_renderer_set_sources(gpu, NULL, 0, sun));
    ASSERT_TRUE(fr_renderer_set_mode(gpu, FR_RENDERER_LIT));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(gpu, 1.f / 60.f));
    uint32_t wood_cell = (91u - 64u) * DC_CHUNK_SIDE + (208u - 192u);
    fr_reactive_record_t wood_before;
    bool found = false;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, 7u, wood_cell, &wood_before,
                                          &found, err, sizeof(err)) && found);
    mkdir("build/screenshots", 0777);
    for (uint32_t tick = 0; tick < 8; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_loaded.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 12; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(gpu, &scene, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu,1,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release_projectile(gpu,&scene,err,sizeof(err)));
    for (uint32_t tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_flight.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 47; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_impact.bmp",
                        err, sizeof(err)));
    dc_gpu_world_body_t coal, wall;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.coal_body, &coal,
                                       err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, scene.wall_body, &wall,
                                       err, sizeof(err)));
    printf("rendered impact coal %.2f wall %.2f\n", xof(coal), xof(wall));
    ASSERT_TRUE(xof(coal) > xof(wall) + 20.f);
    for (uint32_t tick = 0; tick < 130; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_battlement_fire.bmp",
                        err, sizeof(err)));
    fr_reactive_record_t burning;
    ASSERT_TRUE(fr_gpu_reactive_read_cell(gpu, 7u, wood_cell, &burning,
                                          &found, err, sizeof(err)) && found);
    float wood_temperature = 0.f;
    ASSERT_TRUE(fr_reactive_state_temperature(&burning.state,
                                              &wood_temperature, err,
                                              sizeof(err)));
    printf("display battlement %.1fK fuel %u -> %u flags %u\n",
           wood_temperature, wood_before.state.fuel_mass_fp,
           burning.state.fuel_mass_fp, burning.state.flags);
    ASSERT_TRUE((burning.state.flags & FR_REACTIVE_FLAMING) != 0u);
    ASSERT_TRUE(burning.state.fuel_mass_fp < wood_before.state.fuel_mass_fp);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    if (getenv("CATAPULT_DISPLAY_REPEAT_HEADLESS")) {
        RUN(test_hot_coal_clears_wall_and_ignites_wooden_inner_battlement);
        RUN(test_projectile_crosses_a_chunk_before_wall_contact);
        RUN(test_grounded_triangular_frame_and_tension_release);
        RUN(test_cold_launcher_has_supported_frame_and_real_latch);
        RUN(test_hot_coal_load_keeps_finite_fuel_and_checkpointed_joint);
        RUN(test_hot_coal_emits_finite_reaction_without_igniting_cold_control);
        RUN(test_wooden_arm_has_finite_reactive_ownership);
        RUN(test_prompt_hot_launch_keeps_sound_wood_arm_unignited);
        RUN(test_equal_elapsed_cadences_keep_launcher_flight_and_gas_clock);
    }
    RUN(test_render_moving_arm_without_fire_occlusion);
    RUN(test_render_real_loaded_flight_and_impact);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
