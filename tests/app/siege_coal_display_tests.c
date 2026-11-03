#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "../../src/app/view_config.h"
#include "../test_window.h"

static bool siege_window(dc_gpu_t **gpu, uint32_t width, uint32_t height,
                         const char *shader, char *err, uint32_t cap) {
    (void)width;(void)height;
    if (!dc_test_create_window(gpu, SIM_WIDTH, SIM_HEIGHT, 960, 540, shader,
                               err, cap))
        return false;
    return true;
}

#define dc_gpu_create siege_window
#define DC_SIEGE_PRESENTATION 1
#define DC_SIEGE_DISPLAY_INCLUDE
#include "siege_coal_tests.c"
#undef dc_gpu_create

int main(void) {
    mkdir("build/screenshots", 0777);
    RUN(test_locked_siege_scene_matches_the_manifest_fixture);
    RUN(test_wet_siege_control_saturates_the_wall);
    RUN(test_cold_projectile_and_prompt_launch_keep_the_launcher_sound);
    RUN(test_hot_coal_impact_burns_a_restrained_wall_fraction);
    RUN(test_very_wet_wall_resists_sustained_ignition);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
