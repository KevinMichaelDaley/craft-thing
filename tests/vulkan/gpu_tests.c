#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/gpu.h"

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_gpu_pattern_readback(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[0], 0xff202020u);
    ASSERT_EQ(cells[1], 0xff4040c0u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[15], 0xff202020u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_rejects_invalid_dimensions(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(!dc_gpu_create(&gpu, 0, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(gpu == NULL);
    ASSERT_TRUE(strlen(err) > 0);
    PASS();
}

static void test_gpu_brush_updates_only_covered_cells(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint(gpu, 1, 1, 0, 0xff00ff00u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[5], 0xff00ff00u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[6], 0xff4040c0u);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_gpu_pattern_readback);
    RUN(test_rejects_invalid_dimensions);
    RUN(test_gpu_brush_updates_only_covered_cells);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
