#include <stdio.h>
#include <stdlib.h>

#include "native_scene.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_native_residency_covers_partial_edge_chunks(void) {
    ASSERT_EQ(native_scene_count(1920, 1080), 510u);
    ASSERT_EQ(native_scene_count(2048, 1216), 608u);
    ASSERT_EQ(native_scene_count(513, 513), 81u);
    PASS();
}

static void test_scene_has_water_and_stable_grains_without_seam_walls(void) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    native_scene_chunk(chunk, 8u * 30u + 15u, 1920, 1080);
    ASSERT_INT_EQ(chunk->coord.x, 15);
    ASSERT_INT_EQ(chunk->coord.y, 8);
    ASSERT_EQ(chunk->cells[63u * DC_CHUNK_SIDE].material, DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[63u * DC_CHUNK_SIDE].fluid_mass, DC_FLUID_FULL);
    native_scene_chunk(chunk, 14u * 30u + 15u, 1920, 1080);
    ASSERT_TRUE(chunk->particle_count > 0u);
    uint32_t count = chunk->particle_count;
    uint32_t seed = dc_chunk_particle_seed(chunk->coord);
    bool found = false;
    for (uint32_t i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; ++i)
        if (chunk->particles[i].mass_fp) {
            ASSERT_EQ(chunk->particles[i].id_hi, seed);
            found = true;
        }
    ASSERT_TRUE(found);
    native_scene_chunk(chunk, 14u * 30u + 15u, 1920, 1080);
    ASSERT_EQ(chunk->particle_count, count);
    free(chunk);
    PASS();
}

static void test_partial_chunk_excludes_out_of_view_materials(void) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk != NULL);
    native_scene_chunk(chunk, 509u, 1920, 1080);
    ASSERT_INT_EQ(chunk->coord.x, 29);
    ASSERT_INT_EQ(chunk->coord.y, 16);
    ASSERT_EQ(chunk->cells[55u * DC_CHUNK_SIDE].material, DC_MATERIAL_STONE);
    for (uint32_t i = 56u * DC_CHUNK_SIDE; i < DC_CHUNK_CELLS; ++i) {
        ASSERT_EQ(chunk->cells[i].material, DC_MATERIAL_AIR);
        ASSERT_EQ(chunk->cells[i].fluid_mass, 0u);
        ASSERT_EQ(chunk->particles[i].mass_fp, 0u);
    }
    free(chunk);
    PASS();
}

int main(void) {
    RUN(test_native_residency_covers_partial_edge_chunks);
    RUN(test_scene_has_water_and_stable_grains_without_seam_walls);
    RUN(test_partial_chunk_excludes_out_of_view_materials);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
