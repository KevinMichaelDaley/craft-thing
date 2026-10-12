#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"
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
int main(void) {
    RUN(test_native_painted_island_falls_without_body_spawn);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
