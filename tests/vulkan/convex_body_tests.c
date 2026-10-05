#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *grid(void) {
    char err[256];
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool okay = dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err));
    for (uint32_t x = 0; okay && x < 2; ++x)
        okay = dc_gpu_upload_chunk(gpu, x, chunk, err, sizeof(err)) &&
               dc_gpu_set_page(gpu, x, 0, x, err, sizeof(err));
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_world_body_t body(uint32_t id) {
    return (dc_gpu_world_body_t){ .body = { .x_fp = 62 << 16, .y_fp = 10 << 16,
        .width = 4, .height = 4, .id = id, .active = 1 } };
}

static dc_gpu_body_shape_t diamond(uint32_t material) {
    return (dc_gpu_body_shape_t){ .count = 4, .material = material,
        .vertices = {{2 << 16, 0}, {4 << 16, 2 << 16},
                     {2 << 16, 4 << 16}, {0, 2 << 16}} };
}

static void test_rotated_stone_and_wood_rasterize_across_chunk_seam(void) {
    char err[256];
    dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    for (uint32_t material = DC_GPU_BODY_STONE; material <= DC_GPU_BODY_WOOD; ++material) {
        dc_gpu_body_shape_t shape = diamond(material);
        ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(13), &shape, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
        for (uint32_t y = 0; y < 4; ++y) for (uint32_t x = 0; x < 4; ++x) {
            uint32_t current, swept;
            ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 62 + x, 10 + y, &current, &swept, err, sizeof(err)));
            bool corner = (x == 0 || x == 3) && (y == 0 || y == 3);
            ASSERT_EQ(current, corner ? 0u : 13u);
            ASSERT_EQ(swept, 13u);
        }
        dc_gpu_body_shape_t read;
        ASSERT_TRUE(dc_gpu_read_body_shape(gpu, 13, &read, err, sizeof(err)));
        ASSERT_EQ(read.material, material); ASSERT_EQ(read.count, 4u);
    }
    dc_gpu_destroy(gpu); PASS();
}

static void test_one_cell_edge_overlap_is_not_a_center_sample(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_gpu_body_shape_t shape = { .count = 3, .material = DC_GPU_BODY_STONE,
        .vertices = {{0, 0}, {4 << 16, 0}, {0, 1 << 14}} };
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(17), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 65, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 17u);
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 62, 11, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_invalid_shapes_leave_existing_body_unchanged(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_gpu_body_shape_t good = diamond(DC_GPU_BODY_WOOD), bad, read;
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(23), &good, err, sizeof(err)));
    for (uint32_t scenario = 0; scenario < 7; ++scenario) {
        bad = good;
        switch (scenario) {
            case 0: bad.count = 2; break;
            case 1: bad.count = DC_GPU_CONVEX_VERTICES + 1; break;
            case 2: bad.material = 99; break;
            case 3: bad.vertices[0].x_fp = -1; break;
            case 4: bad.vertices[1].x_fp = (4 << 16) + 1; break;
            case 5: bad.vertices[2] = bad.vertices[1]; break;
            case 6: bad.vertices[2].y_fp = 1 << 16; break;
        }
        dc_gpu_world_body_t changed = body(23); changed.body.x_fp = 1 << 16;
        ASSERT_TRUE(!dc_gpu_spawn_convex_body(gpu, changed, &bad, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_shape(gpu, 23, &read, err, sizeof(err)));
        ASSERT_EQ(memcmp(&read, &good, sizeof(read)), 0);
        dc_gpu_world_body_t world;
        ASSERT_TRUE(dc_gpu_read_world_body(gpu, 23, &world, err, sizeof(err)));
        ASSERT_EQ(world.body.x_fp, 62 << 16);
    }
    read = good;
    ASSERT_TRUE(!dc_gpu_read_body_shape(gpu, 99, &read, err, sizeof(err)));
    ASSERT_EQ(memcmp(&read, &good, sizeof(read)), 0);
    dc_gpu_destroy(gpu); PASS();
}

static void test_far_world_rebase_keeps_shape_and_conservative_candidates(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_chunk_coord_t origin = {INT64_C(1) << 40, -(INT64_C(1) << 40)};
    dc_gpu_world_body_t world = body(31); world.chunk = origin;
    dc_gpu_body_shape_t shape = diamond(DC_GPU_BODY_STONE);
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, world, &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[16];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    ASSERT_EQ(stats.overflow, 0u);
    bool left = false, right = false;
    for (uint32_t i = 0; i < stats.count; ++i) {
        if (pairs[i].kind != DC_GPU_PAIR_TERRAIN) continue;
        if (pairs[i].chunk.x == origin.x) left = true;
        if (pairs[i].chunk.x == origin.x + 1) right = true;
    }
    ASSERT_TRUE(left && right);
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){origin.x + 1, origin.y}, err, sizeof(err)));
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 0, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 31u);
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 31, &world, err, sizeof(err)));
    ASSERT_EQ(world.chunk.x, origin.x); ASSERT_EQ(world.body.x_fp, 62 << 16);
    dc_gpu_destroy(gpu); PASS();
}

static void test_shape_snapshot_roundtrip_and_legacy_box_loading(void) {
    char directory[] = "build/convex_XXXXXX", path[256], err[256];
    ASSERT_TRUE(mkdtemp(directory));
    snprintf(path, sizeof(path), "%s/bodies.bin", directory);
    dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_gpu_body_shape_t shape = diamond(DC_GPU_BODY_WOOD), read;
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(41), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_save_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 41, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_load_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(gpu, 41, &read, err, sizeof(err)));
    ASSERT_EQ(memcmp(&read, &shape, sizeof(shape)), 0);
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 62, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    FILE *file = fopen(path, "wb"); ASSERT_TRUE(file);
    const struct { char magic[4]; uint32_t version, count, reserved; } header = {"DCB1", 1, 1, 0};
    dc_gpu_world_body_t legacy = body(42);
    ASSERT_EQ(fwrite(&header, sizeof(header), 1, file), 1u);
    ASSERT_EQ(fwrite(&legacy, sizeof(legacy), 1, file), 1u);
    ASSERT_INT_EQ(fclose(file), 0);
    ASSERT_TRUE(dc_gpu_load_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(gpu, 42, &read, err, sizeof(err)));
    ASSERT_EQ(read.count, 0u);
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 62, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 42u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_reused_slot_and_box_upsert_clear_old_polygon(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_gpu_body_shape_t shape = diamond(DC_GPU_BODY_STONE), read;
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(51), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 51, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, body(52), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(gpu, 52, &read, err, sizeof(err)));
    ASSERT_EQ(read.count, 0u);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, body(52), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, body(52), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 62, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 52u);
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_rotated_stone_and_wood_rasterize_across_chunk_seam);
    RUN(test_one_cell_edge_overlap_is_not_a_center_sample);
    RUN(test_invalid_shapes_leave_existing_body_unchanged);
    RUN(test_far_world_rebase_keeps_shape_and_conservative_candidates);
    RUN(test_shape_snapshot_roundtrip_and_legacy_box_loading);
    RUN(test_reused_slot_and_box_upsert_clear_old_polygon);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
