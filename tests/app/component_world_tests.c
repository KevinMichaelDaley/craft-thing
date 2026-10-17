#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"
#include "../../src/app/prop_scene.h"
#include "../../src/app/view_config.h"
#include "ferrum/structural/gpu.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_native_painted_island_falls_without_body_spawn(void) {
    char directory[] = "build/ui_component_world_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view =
        dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 32, 32, 6, DC_MATERIAL_AIR, err,
                                    sizeof(err)));
    ASSERT_TRUE(dc_level_view_paint(view, 32, 32, 1, DC_MATERIAL_STONE, err,
                                    sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    dc_gpu_world_body_t body;
    dc_gpu_body_motion_t motion;
    bool extracted = dc_level_view_body_state(view, 0x80000001u, &body, &motion,
                                              err, sizeof(err));
    bool falling = extracted && body.body.vy_fp > 0;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(extracted);
    ASSERT_TRUE(falling);
    PASS();
}
static void test_imported_tree_falls_after_air_cut(void) {
    char directory[] = "build/ui_tree_cut_XXXXXX", err[256] = {0};
    const char *asset = "assets/props/level_piece01.prop/asset.cprop";
    ASSERT_TRUE(mkdtemp(directory));
    dc_level_view_t *view = dc_prop_scene_create(directory, 314, asset, err, sizeof(err));
    ASSERT_TRUE(view);
    ASSERT_TRUE(dc_prop_scene_load(view, asset, err, sizeof(err)));
    /* Scene camera is centered on world (32,32). Cut the entire trunk. */
    for (int x = 8; x <= 30; x += 2)
        ASSERT_TRUE(dc_level_view_paint(view, VIEW_WIDTH/2+x-32,
            VIEW_HEIGHT/2+29-32, 3, DC_MATERIAL_AIR, err, sizeof(err)));
    for (unsigned tick = 0; tick < 4; tick++) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    bool tree = false;
    for (unsigned i = 1; i <= 64; i++) {
        dc_gpu_world_body_t body;
        dc_gpu_body_motion_t motion;
        if (dc_level_view_body_state(view, 0x80000000u+i, &body, &motion, err, sizeof(err)) &&
            body.body.width >= 25 && body.body.height >= 40 && body.body.vy_fp > 0)
            tree = true;
    }
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(tree);
    PASS();
}
int main(void) {
    RUN(test_native_painted_island_falls_without_body_spawn);
    RUN(test_imported_tree_falls_after_air_cut);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
