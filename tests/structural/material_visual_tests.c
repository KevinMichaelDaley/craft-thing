#define main placement_fixture_main
#include "placement_tests.c"
#undef main
#include <SDL.h>

static bool g_scene_generated;
static void test_material_failure_and_buoyancy_have_actual_visual_artifacts(void) {
    ASSERT_TRUE(g_scene_generated);
    PASS();
}
int main(void) {
    RUN(test_material_failure_and_buoyancy_have_actual_visual_artifacts);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
