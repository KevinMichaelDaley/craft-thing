#include <SDL.h>
#include <sys/stat.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/renderer/gpu.h"

static bool catapult_window(dc_gpu_t **gpu, uint32_t width, uint32_t height,
                            const char *shader, char *err, uint32_t cap) {
    return dc_gpu_create_window(gpu, width, height, 1280, 720,
                                shader, err, cap) &&
           dc_gpu_set_presentation_focus(*gpu,
        (dc_presentation_focus_t){45, 64, 50, 36});
}

#define dc_gpu_create catapult_window
#define DC_CATAPULT_DISPLAY_INCLUDE
#include "catapult_scene_tests.c"
#undef dc_gpu_create

static bool capture(dc_gpu_t *gpu, const char *path, char *err,
                    uint32_t cap) {
    if (!dc_gpu_present_chunks_steps(gpu, 0, err, cap)) return false;
    uint32_t *pixels = calloc(128u * 128u, sizeof(*pixels));
    if (!pixels) return false;
    bool okay = dc_gpu_readback(gpu, pixels, 128u * 128u, err, cap);
    SDL_Surface *source = okay ? SDL_CreateRGBSurfaceWithFormatFrom(
        pixels + 64u * 128u + 45u, 50, 36, 32, 128 * 4,
        SDL_PIXELFORMAT_ABGR8888) : NULL;
    SDL_Surface *image = source ? SDL_CreateRGBSurfaceWithFormat(
        0, 1000, 720, 32, SDL_PIXELFORMAT_ABGR8888) : NULL;
    okay = image && SDL_BlitScaled(source, NULL, image, NULL) == 0 &&
           SDL_SaveBMP(image, path) == 0;
    if (image) SDL_FreeSurface(image);
    if (source) SDL_FreeSurface(source);
    free(pixels);
    return okay;
}

static void test_render_moving_arm_without_fire_occlusion(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid();
    ASSERT_TRUE(gpu);
    dc_catapult_scene_t scene = {0};
    ASSERT_TRUE(dc_catapult_scene_build(gpu, false, &scene, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_enable(gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(gpu, FR_RENDERER_ALBEDO));
    mkdir("build/screenshots", 0777);
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_loaded.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 20; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_cocked.bmp",
                        err, sizeof(err)));
    uint32_t pixels[128u * 128u];
    ASSERT_TRUE(dc_gpu_readback(gpu,pixels,128u*128u,err,sizeof(err)));
    uint32_t rear_red=pixels[78u*128u+68u]&255u;
    uint32_t tip_red=pixels[78u*128u+76u]&255u;
    printf("cocked arm red channels rear %u tip %u\n",rear_red,tip_red);
    ASSERT_TRUE(rear_red>70u&&tip_red>70u);
    dc_gpu_body_motion_t held,launched;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,scene.arm_body,&held,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(gpu, &scene, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,scene.arm_body,&launched,
        err,sizeof(err)));
    ASSERT_TRUE(launched.angle>held.angle+.1f);
    ASSERT_TRUE(dc_catapult_scene_release_projectile(gpu,&scene,err,sizeof(err)));
    for (uint32_t tick = 0; tick < 8; tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_cold_flight.bmp",
                        err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu,pixels,128u*128u,err,sizeof(err)));
    ASSERT_TRUE((pixels[79u*128u+76u]&255u)>70u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_render_real_loaded_flight_and_impact(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid();
    ASSERT_TRUE(gpu);
    dc_catapult_scene_t scene = {0};
    ASSERT_TRUE(dc_catapult_scene_build(gpu, true, &scene, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_enable(gpu, err, sizeof(err)));
    ASSERT_TRUE(fr_renderer_set_mode(gpu, FR_RENDERER_ALBEDO));
    ASSERT_TRUE(dc_gpu_set_tick_seconds(gpu, 1.f / 60.f));
    mkdir("build/screenshots", 0777);
    for (uint32_t tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_loaded.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 10; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(gpu, &scene, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu,1,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release_projectile(gpu,&scene,err,sizeof(err)));
    for (uint32_t tick = 0; tick < 3; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_flight.bmp",
                        err, sizeof(err)));
    for (uint32_t tick = 0; tick < 5; tick++)
        ASSERT_TRUE(dc_gpu_present_chunks_steps(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(capture(gpu, "build/screenshots/catapult_impact.bmp",
                        err, sizeof(err)));
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    if (getenv("CATAPULT_DISPLAY_REPEAT_HEADLESS")) {
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
