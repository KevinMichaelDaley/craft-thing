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

static dc_gpu_t *make_grid(uint32_t height, char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool okay = dc_gpu_create(&gpu, 128, height, "build/shaders/pattern.comp.spv", err, cap);
    for (uint32_t tile_y = 0; okay && tile_y < height / DC_CHUNK_SIDE; ++tile_y) {
        for (uint32_t tile_x = 0; okay && tile_x < 2u; ++tile_x) {
            memset(chunk, 0, sizeof(*chunk));
            chunk->coord = (dc_chunk_coord_t){tile_x, tile_y};
            for (uint32_t y = 0; y < DC_CHUNK_SIDE; ++y)
                if (tile_y * DC_CHUNK_SIDE + y == height - 24u)
                    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x)
                        chunk->cells[y * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
            uint32_t slot = tile_y * 2u + tile_x;
            okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, cap) &&
                   dc_gpu_set_page(gpu, tile_x, tile_y, slot, err, cap);
        }
    }
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_body_t box(uint32_t id, uint32_t x, uint32_t y) {
    return (dc_gpu_body_t){.x_fp = (int32_t)(x * DC_FLUID_FULL),
        .y_fp = (int32_t)(y * DC_FLUID_FULL), .width = 1, .height = 1,
        .id = id, .active = 1};
}

static void test_multiple_ids_integrate_independently_across_chunk_edge(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_body_t first = box(101, 63, 2), second = box(202, 10, 2), saved;
    first.width = first.height = second.width = second.height = 2u;
    first.vx_fp = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, first, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, second, err, sizeof(err)));
    for (uint32_t tick = 0; tick < 20u; ++tick)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 101, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.x_fp, 83 * (int32_t)DC_FLUID_FULL);
    ASSERT_EQ(saved.y_fp, 38 * (int32_t)DC_FLUID_FULL);
    ASSERT_EQ(saved.vy_fp, 0);
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 202, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.x_fp, 10 * (int32_t)DC_FLUID_FULL);
    ASSERT_EQ(saved.y_fp, 38 * (int32_t)DC_FLUID_FULL);
    ASSERT_TRUE(dc_gpu_read_body(gpu, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.id, 101u);
    uint32_t current;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 83, 38, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 101u);
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 10, 38, &current, NULL, err, sizeof(err)));
    ASSERT_EQ(current, 202u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_updating_one_id_preserves_other_bodies(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(11, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(22, 20, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(11, 30, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t saved;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 22, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.x_fp, 20 * (int32_t)DC_FLUID_FULL);
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 11, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.x_fp, 30 * (int32_t)DC_FLUID_FULL);
    uint32_t current, swept;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 10, 2, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    ASSERT_EQ(swept, 0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_pool_capacity_removal_and_slot_reuse_keep_ids_stable(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t id = 1; id <= DC_GPU_BODY_CAPACITY; ++id)
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(id, id, 2), err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_spawn_body(gpu, box(1001, 100, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(64, 80, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 32, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(1001, 100, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t saved = {.id = 999};
    ASSERT_TRUE(!dc_gpu_read_body_id(gpu, 32, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.id, 999u);
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 1001, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.x_fp, 100 * (int32_t)DC_FLUID_FULL);
    for (uint32_t id = 1; id <= DC_GPU_BODY_CAPACITY; ++id) {
        if (id == 32u) continue;
        ASSERT_TRUE(dc_gpu_read_body_id(gpu, id, &saved, err, sizeof(err)));
        ASSERT_EQ(saved.id, id);
    }
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_horizontal_sweep_covers_fast_chunk_crossing_then_clears(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_body_t moving = box(7, 63, 10);
    moving.vx_fp = 4 * (int32_t)DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, moving, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    for (uint32_t x = 63; x <= 67; ++x) {
        uint32_t current, swept;
        ASSERT_TRUE(dc_gpu_read_occupancy(gpu, x, 10, &current, &swept, err, sizeof(err)));
        ASSERT_EQ(current, x == 67u ? 7u : 0u);
        ASSERT_EQ(swept, 7u);
    }
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    uint32_t current, swept;
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 63, 10, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    ASSERT_EQ(swept, 0u);
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 7, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 71, 10, &current, &swept, err, sizeof(err)));
    ASSERT_EQ(current, 0u);
    ASSERT_EQ(swept, 0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_vertical_sweep_covers_chunk_seam(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(128, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_body_t moving = box(17, 20, 63);
    moving.vy_fp = 4 * (int32_t)DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, moving, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    for (uint32_t y = 63; y <= 67; ++y) {
        uint32_t current, swept;
        ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 20, y, &current, &swept, err, sizeof(err)));
        ASSERT_EQ(current, y == 67u ? 17u : 0u);
        ASSERT_EQ(swept, 17u);
    }
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_overlap_ownership_uses_lowest_id_in_both_spawn_orders(void) {
    char err[256] = {0};
    for (uint32_t order = 0; order < 2u; ++order) {
        dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
        ASSERT_TRUE(gpu != NULL);
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(order ? 3u : 9u, 20, 10), err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(order ? 9u : 3u, 20, 10), err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
        uint32_t current, swept;
        ASSERT_TRUE(dc_gpu_read_occupancy(gpu, 20, 10, &current, &swept, err, sizeof(err)));
        ASSERT_EQ(current, 3u);
        ASSERT_EQ(swept, 3u);
        dc_gpu_destroy(gpu);
    }
    PASS();
}

static void test_invalid_ids_and_reads_do_not_mutate_body_state(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = make_grid(64, err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(!dc_gpu_spawn_body(gpu, box(0, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_spawn_body(gpu, box(14, 128, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(UINT32_MAX, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(!dc_gpu_remove_body(gpu, 123, err, sizeof(err)));
    dc_gpu_body_t saved;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, UINT32_MAX, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.id, UINT32_MAX);
    ASSERT_TRUE(!dc_gpu_read_occupancy(gpu, 128, 0, NULL, &saved.id, err, sizeof(err)));
    ASSERT_EQ(saved.id, UINT32_MAX);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_multiple_ids_integrate_independently_across_chunk_edge);
    RUN(test_updating_one_id_preserves_other_bodies);
    RUN(test_pool_capacity_removal_and_slot_reuse_keep_ids_stable);
    RUN(test_horizontal_sweep_covers_fast_chunk_crossing_then_clears);
    RUN(test_vertical_sweep_covers_chunk_seam);
    RUN(test_overlap_ownership_uses_lowest_id_in_both_spawn_orders);
    RUN(test_invalid_ids_and_reads_do_not_mutate_body_state);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
