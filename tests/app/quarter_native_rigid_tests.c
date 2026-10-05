#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"
#include "../../src/app/view_config.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_quarter_native_window_keeps_both_spawned_boxes_visible(void) {
    char directory[] = "build/ui_multi_rigid_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 63, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 122, 2, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 20u; ++tick) {
        ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
        ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    }
    uint32_t first = 0, second = 0, old = 0;
    ASSERT_TRUE(dc_level_view_pixel(view, 83, 52, &first, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 142, 52, &second, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 63, 2, &old, err, sizeof(err)));
    bool rendered = first == 0xff30c040u && second == 0xff30c040u &&
                    old != 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    printf("two-box window pixels: %08x %08x, old footprint %08x\n", first, second, old);
    ASSERT_TRUE(rendered);
    PASS();
}

static void test_world_boxes_survive_camera_eviction_and_session_reload(void) {
    char directory[] = "build/ui_world_rigid_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_spawn_body(view, 63, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t color;
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    ASSERT_EQ(color, 0xff30c040u);
    ASSERT_TRUE(dc_level_view_pan_pixels(view, 16 * DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    bool evicted = !dc_level_view_has_chunk(view, (dc_chunk_coord_t){0, INITIAL_CHUNK_Y});
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    bool clipped = color != 0xff30c040u;
    ASSERT_TRUE(dc_level_view_pan_pixels(view, -16 * (int32_t)DC_CHUNK_SIDE, 0));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    bool restored = color == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(evicted && clipped && restored);
    view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_set_spring_enabled(view, false));
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 64, 2, &color, err, sizeof(err)));
    restored = color == 0xff30c040u;
    ASSERT_TRUE(dc_level_view_spawn_body(view, 122, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    uint32_t second;
    ASSERT_TRUE(dc_level_view_pixel(view, 65, 2, &color, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_pixel(view, 123, 2, &second, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    ASSERT_TRUE(restored);
    ASSERT_EQ(color, 0xff30c040u); ASSERT_EQ(second, 0xff30c040u);
    PASS();
}

int main(void) {
    RUN(test_quarter_native_window_keeps_both_spawned_boxes_visible);
    RUN(test_world_boxes_survive_camera_eviction_and_session_reload);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
