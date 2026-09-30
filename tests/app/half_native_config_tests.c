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

static void test_half_native_view_uses_one_simulated_cell_per_two_screen_pixels(void) {
    ASSERT_EQ(VIEW_WIDTH, 960);
    ASSERT_EQ(VIEW_HEIGHT, 540);
    ASSERT_EQ(WINDOW_SCALE, 2);
    ASSERT_EQ(BRUSH_RADIUS, 6);
    ASSERT_EQ(SPRING_RADIUS, 2);
    ASSERT_EQ(WORLD_SCALE * WINDOW_SCALE, 4);
    PASS();
}

static void test_half_native_resident_chunk_grid_and_fluid_cadence(void) {
    ASSERT_EQ(SIM_WIDTH, 1088);
    ASSERT_EQ(SIM_HEIGHT, 704);
    ASSERT_EQ(SIM_CHUNKS_X * SIM_CHUNKS_Y, 187);
    ASSERT_EQ(DC_GPU_CHUNK_SLOTS, 187u);
    ASSERT_EQ(FLUID_INTERVAL, 3);
    ASSERT_INT_EQ(INITIAL_CHUNK_Y, -4);
    PASS();
}

int main(void) {
    RUN(test_half_native_view_uses_one_simulated_cell_per_two_screen_pixels);
    RUN(test_half_native_resident_chunk_grid_and_fluid_cadence);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
