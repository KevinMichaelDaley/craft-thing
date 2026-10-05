#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static dc_gpu_t *grid(char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool okay = dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, cap);
    for (uint32_t x = 0; okay && x < 2; ++x) {
        chunk->coord.x = x;
        okay = dc_gpu_upload_chunk(gpu, x, chunk, err, cap) &&
               dc_gpu_set_page(gpu, x, 0, x, err, cap);
    }
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_world_body_t world_box(dc_chunk_coord_t chunk, uint32_t id) {
    return (dc_gpu_world_body_t){ .chunk = chunk,
        .body = { .x_fp = 63 << 16, .y_fp = 10 << 16,
            .vx_fp = 4 << 16, .width = 1, .height = 1, .id = id, .active = 1 } };
}

static void test_world_motion_survives_far_camera_origin_and_page_eviction(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_chunk_coord_t origin = { (INT64_C(1) << 40) - 1, -(INT64_C(1) << 40) };
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, world_box(origin, 19), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_world_body_t before, after;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 19, &before, err, sizeof(err)));
    ASSERT_EQ(before.chunk.x, origin.x + 1);
    ASSERT_EQ(before.chunk.y, origin.y);
    ASSERT_EQ(before.body.x_fp, 3 << 16);
    ASSERT_EQ(before.body.y_fp, (10 << 16) + 16384);
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){-origin.x, -origin.y},
                                      err, sizeof(err)));
    uint32_t current = 123, swept = 123;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 67, 10, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u); ASSERT_EQ(swept, 0u);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 19, &after, err, sizeof(err)));
    ASSERT_EQ(after.chunk.x, before.chunk.x); ASSERT_EQ(after.chunk.y, before.chunk.y);
    ASSERT_EQ(after.body.x_fp, before.body.x_fp); ASSERT_EQ(after.body.y_fp, before.body.y_fp);
    ASSERT_EQ(after.body.vy_fp, before.body.vy_fp);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 67, 10, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 19u); ASSERT_EQ(swept, 19u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_signed_world_chunk_carry_preserves_full_coordinate_range(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_chunk_coord_t origin = { INT64_MAX - 1, INT64_MIN + 1 };
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, world_box(origin, 29), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_world_body_t saved;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 29, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.chunk.x, INT64_MAX); ASSERT_EQ(saved.chunk.y, INT64_MIN + 1);
    ASSERT_EQ(saved.body.x_fp, 3 << 16);
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){INT64_MIN, INT64_MAX},
                                      err, sizeof(err)));
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 67, 10, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 29, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.chunk.x, INT64_MAX);
    dc_gpu_destroy(gpu); PASS();
}

static void test_snapshot_restores_ids_velocities_and_rejects_truncation_atomically(void) {
    char directory[] = "build/world_bodies_XXXXXX", path[256], err[256] = {0};
    ASSERT_TRUE(mkdtemp(directory) != NULL);
    snprintf(path, sizeof(path), "%s/rigid_bodies.bin", directory);
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_chunk_coord_t origin = {-2, -3};
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, world_box(origin, 41), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, world_box((dc_chunk_coord_t){999, -1000}, 42),
                                       err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_world_body_t before, after;
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 41, &before, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_save_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 41, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 42, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_load_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 41, &after, err, sizeof(err)));
    ASSERT_EQ(after.chunk.x, before.chunk.x); ASSERT_EQ(after.chunk.y, before.chunk.y);
    ASSERT_EQ(after.body.x_fp, before.body.x_fp); ASSERT_EQ(after.body.y_fp, before.body.y_fp);
    ASSERT_EQ(after.body.vx_fp, before.body.vx_fp); ASSERT_EQ(after.body.vy_fp, before.body.vy_fp);
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 42, &after, err, sizeof(err)));
    ASSERT_EQ(after.chunk.x, 999); ASSERT_EQ(after.chunk.y, -1000);
    FILE *file = fopen(path, "wb");
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ(fwrite("DCB1", 1, 4, file), 4u);
    ASSERT_INT_EQ(fclose(file), 0);
    ASSERT_TRUE(!dc_gpu_load_bodies(gpu, path, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(gpu, 41, &after, err, sizeof(err)));
    ASSERT_EQ(after.body.x_fp, before.body.x_fp);
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_world_motion_survives_far_camera_origin_and_page_eviction);
    RUN(test_signed_world_chunk_carry_preserves_full_coordinate_range);
    RUN(test_snapshot_restores_ids_velocities_and_rejects_truncation_atomically);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
