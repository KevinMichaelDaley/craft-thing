#include <stdio.h>

#include "../../src/app/view_config.h"
#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_quarter_native_uses_four_screen_pixels_per_cell(void) {
    ASSERT_EQ(VIEW_WIDTH, 480);
    ASSERT_EQ(VIEW_HEIGHT, 270);
    ASSERT_EQ(WINDOW_SCALE, 4);
    ASSERT_EQ(BRUSH_RADIUS, 3);
    ASSERT_EQ(WORLD_SCALE * WINDOW_SCALE, 4);
    PASS();
}

static void test_quarter_native_chunk_grid_and_fluid_cadence(void) {
    ASSERT_EQ(SIM_WIDTH, 640);
    ASSERT_EQ(SIM_HEIGHT, 448);
    ASSERT_EQ(SIM_CHUNKS_X * SIM_CHUNKS_Y, 70);
    ASSERT_EQ(DC_GPU_CHUNK_SLOTS, 70u);
    ASSERT_EQ(FLUID_INTERVAL, 1);
    ASSERT_INT_EQ(INITIAL_CHUNK_Y, -2);
    PASS();
}

int main(void) {
    RUN(test_quarter_native_uses_four_screen_pixels_per_cell);
    RUN(test_quarter_native_chunk_grid_and_fluid_cadence);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
