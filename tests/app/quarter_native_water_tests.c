#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../src/app/level.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static uint64_t source_mass(const dc_chunk_t *chunk) {
    uint64_t mass = 0u;
    for (uint32_t y = 0u; y < 10u; ++y)
        for (uint32_t x = 0u; x < 5u; ++x)
            mass += chunk->cells[y * DC_CHUNK_SIDE + x].fluid_mass;
    return mass;
}

static void test_quarter_native_spring_adds_bounded_water_per_tick(void) {
    char directory[] = "build/ui_quarter_water_XXXXXX", err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    dc_level_view_t *view = dc_level_view_create(directory, 314, err, sizeof(err));
    ASSERT_TRUE(view != NULL);
    ASSERT_TRUE(dc_level_view_wait_visible(view, 120000u, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){2, 0},
                                    chunk, err, sizeof(err)));
    uint64_t before = source_mass(chunk);
    ASSERT_TRUE(dc_level_view_step(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_tick(view, err, sizeof(err)));
    ASSERT_TRUE(dc_level_view_chunk(view, (dc_chunk_coord_t){2, 0},
                                    chunk, err, sizeof(err)));
    uint64_t after = source_mass(chunk);
    printf("quarter-native spring source gain %.2f cells\n",
           (double)(after - before) / DC_FLUID_FULL);
    ASSERT_TRUE(after >= before + 3u * (uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(after <= before + 6u * (uint64_t)DC_FLUID_FULL);
    free(chunk);
    ASSERT_TRUE(dc_level_view_destroy(view, err, sizeof(err)));
    PASS();
}

int main(void) {
    RUN(test_quarter_native_spring_adds_bounded_water_per_tick);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
