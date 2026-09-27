#include <stdio.h>
#include <string.h>

#include "dungeoncraft/generate.h"

static int g_pass = 0;
static int g_fail = 0;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static uint32_t surface(const dc_chunk_t *chunk, uint32_t x) {
    for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
        if (chunk->cells[y * DC_CHUNK_SIDE + x].material == DC_MATERIAL_STONE) return y;
    return DC_CHUNK_SIDE;
}

static void test_generation_is_reproducible_and_border_is_smooth(void) {
    dc_chunk_t a, repeat, neighbor;
    dc_generate_chunk(314, (dc_chunk_coord_t){-2, 0}, &a);
    dc_generate_chunk(314, (dc_chunk_coord_t){-2, 0}, &repeat);
    dc_generate_chunk(314, (dc_chunk_coord_t){-1, 0}, &neighbor);
    ASSERT_EQ(memcmp(&a, &repeat, sizeof(a)), 0);
    uint32_t left = surface(&a, 63), right = surface(&neighbor, 0);
    ASSERT_TRUE(left <= right + 1 && right <= left + 1);
    PASS();
}

static void test_cave_and_basin_span_chunk_boundaries(void) {
    dc_chunk_t left, right, cave_left, cave_right;
    dc_generate_chunk(314, (dc_chunk_coord_t){1, 0}, &left);
    dc_generate_chunk(314, (dc_chunk_coord_t){2, 0}, &right);
    dc_generate_chunk(314, (dc_chunk_coord_t){1, 1}, &cave_left);
    dc_generate_chunk(314, (dc_chunk_coord_t){2, 1}, &cave_right);
    ASSERT_EQ(left.cells[40 * DC_CHUNK_SIDE + 63].fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(right.cells[40 * DC_CHUNK_SIDE + 0].fluid_mass, DC_FLUID_FULL);
    ASSERT_EQ(cave_left.cells[6 * DC_CHUNK_SIDE + 63].material, DC_MATERIAL_AIR);
    ASSERT_EQ(cave_right.cells[6 * DC_CHUNK_SIDE + 0].material, DC_MATERIAL_AIR);
    PASS();
}

static void test_scaled_surface_uses_four_times_as_many_simulated_rows(void) {
    dc_chunk_t base, scaled;
    dc_generate_chunk(314, (dc_chunk_coord_t){0, 0}, &base);
    uint32_t old_height = surface(&base, 0);
    uint32_t new_height = DC_CHUNK_SIDE * 4u;
    for (uint32_t cy = 0; cy < 4u; ++cy) {
        ASSERT_TRUE(dc_generate_chunk_scaled(314,
                    (dc_chunk_coord_t){0, (int64_t)cy}, 4u, &scaled));
        uint32_t local = surface(&scaled, 0);
        if (local < DC_CHUNK_SIDE && new_height == DC_CHUNK_SIDE * 4u)
            new_height = cy * DC_CHUNK_SIDE + local;
    }
    ASSERT_EQ(new_height, old_height * 4u);
    ASSERT_TRUE(!dc_generate_chunk_scaled(314, (dc_chunk_coord_t){0, 0},
                                          0u, &scaled));
    PASS();
}

int main(void) {
    RUN(test_generation_is_reproducible_and_border_is_smooth);
    RUN(test_cave_and_basin_span_chunk_boundaries);
    RUN(test_scaled_surface_uses_four_times_as_many_simulated_rows);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
