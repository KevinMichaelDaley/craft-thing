#include <SDL.h>

#include "dungeoncraft/gpu.h"
#include "../../src/app/view_config.h"
#include "../test_window.h"

static bool forest_window(dc_gpu_t **gpu, uint32_t width, uint32_t height,
                          const char *shader, char *err, uint32_t cap) {
    (void)width;(void)height;
    if (!dc_test_create_window(gpu, SIM_WIDTH, SIM_HEIGHT, 960, 540, shader,
                               err, cap))
        return false;
    return true;
}

#define dc_gpu_create forest_window
#define DC_FOREST_PRESENTATION 1
#define DC_FOREST_DISPLAY_INCLUDE
#include "forest_bonfire_tests.c"
#undef dc_gpu_create

int main(void) {
    mkdir("build/screenshots", 0777);
    RUN(test_locked_forest_scene_matches_the_manifest_fixture);
    RUN(test_still_forest_bonfire_warms_stays_contained_and_keeps_finite_ledgers_180s);
    RUN(test_east_crosswind_keeps_fire_inside_the_cleared_ring);
    RUN(test_west_crosswind_keeps_fire_inside_the_cleared_ring);
    RUN(test_sealed_pit_blaze_starves_then_recovers_when_vented);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
