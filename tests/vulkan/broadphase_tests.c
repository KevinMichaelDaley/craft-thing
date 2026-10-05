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

static dc_gpu_t *grid(char *err, uint32_t cap) {
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool okay = dc_gpu_create(&gpu, 128, 128, "build/shaders/pattern.comp.spv", err, cap);
    for (uint32_t slot = 0; okay && slot < 4; ++slot) {
        chunk->coord = (dc_chunk_coord_t){slot % 2, slot / 2};
        for (uint32_t i = 0; i < DC_CHUNK_CELLS; ++i)
            chunk->cells[i].material = slot >= 2 && i / DC_CHUNK_SIDE == 16 ?
                                       DC_MATERIAL_STONE : DC_MATERIAL_AIR;
        okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, cap) &&
               dc_gpu_set_page(gpu, slot % 2, slot / 2, slot, err, cap);
    }
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_body_t box(uint32_t id, uint32_t x, uint32_t y, uint32_t side) {
    return (dc_gpu_body_t){ .x_fp = (int32_t)(x << 16), .y_fp = (int32_t)(y << 16),
        .width = side, .height = side, .id = id, .active = 1 };
}

static uint32_t body_pairs(const dc_gpu_broadphase_pair_t *pairs, uint32_t count) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < count; ++i) result += pairs[i].kind == DC_GPU_PAIR_BODY;
    return result;
}

static int compare_pair(const void *left, const void *right) {
    const dc_gpu_broadphase_pair_t *a = left, *b = right;
#define CMP(field) do { if (a->field != b->field) return a->field < b->field ? -1 : 1; } while (0)
    CMP(kind); CMP(body_a); CMP(body_b); CMP(chunk.x); CMP(chunk.y);
#undef CMP
    return 0;
}

static void test_dense_pool_reports_every_unique_body_pair(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t id = 1; id <= DC_GPU_BODY_CAPACITY; ++id)
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(id, 20, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[DC_GPU_BROADPHASE_PAIR_CAPACITY];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, DC_GPU_BROADPHASE_PAIR_CAPACITY,
                                     err, sizeof(err)));
    ASSERT_EQ(stats.overflow, 0u); ASSERT_EQ(stats.count, stats.required);
    ASSERT_EQ(stats.active_bodies, 64u);
    ASSERT_EQ(body_pairs(pairs, stats.count), 64u * 63u / 2u);
    qsort(pairs, stats.count, sizeof(*pairs), compare_pair);
    for (uint32_t i = 0; i < stats.count; ++i) {
        if (i) ASSERT_TRUE(compare_pair(&pairs[i - 1], &pairs[i]) != 0);
        if (pairs[i].kind == DC_GPU_PAIR_BODY) {
            ASSERT_TRUE(pairs[i].body_a < pairs[i].body_b);
            ASSERT_TRUE(pairs[i].body_a >= 1 && pairs[i].body_b <= 64);
        }
    }
    dc_gpu_destroy(gpu); PASS();
}

static void test_supported_fast_sweep_is_deduplicated_across_chunk_seam(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_body_t first = box(10, 61, 76, 4), second = box(20, 65, 76, 4);
    first.vx_fp = 4 << 16;
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, first, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, second, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[32];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 32, err, sizeof(err)));
    ASSERT_EQ(body_pairs(pairs, stats.count), 1u);
    bool terrain_left = false, terrain_right = false;
    for (uint32_t i = 0; i < stats.count; ++i) {
        if (pairs[i].kind == DC_GPU_PAIR_BODY) {
            ASSERT_EQ(pairs[i].body_a, 10u); ASSERT_EQ(pairs[i].body_b, 20u);
        } else if (pairs[i].body_a == 10 && pairs[i].chunk.y == 1) {
            terrain_left |= pairs[i].chunk.x == 0;
            terrain_right |= pairs[i].chunk.x == 1;
        }
    }
    ASSERT_TRUE(terrain_left && terrain_right);
    dc_gpu_destroy(gpu); PASS();
}

static void test_crossing_paths_use_swept_bounds_and_expire_next_tick(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_gpu_body_t first = box(11, 62, 79, 1), second = box(22, 65, 79, 1);
    first.vx_fp = 4 << 16; second.vx_fp = -(4 << 16);
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, first, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, second, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[16];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    ASSERT_EQ(body_pairs(pairs, stats.count), 1u);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    ASSERT_EQ(body_pairs(pairs, stats.count), 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_nonoverlapping_bodies_in_one_bucket_do_not_form_pair(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(1, 2, 20, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(2, 30, 20, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[16];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    ASSERT_EQ(stats.active_bodies, 2u);
    ASSERT_EQ(body_pairs(pairs, stats.count), 0u);
    ASSERT_TRUE(stats.count >= 2u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_world_pair_keys_survive_camera_and_terrain_slot_reuse(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    dc_chunk_coord_t origin = { (INT64_C(1) << 40) - 1, -(INT64_C(1) << 40) };
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(99, 65, 20, 8), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(33, 68, 20, 8), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t before, after;
    dc_gpu_broadphase_pair_t first[32], second[32];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &before, first, 32, err, sizeof(err)));
    ASSERT_EQ(body_pairs(first, before.count), 1u);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 3, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 2, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){origin.x + 1, origin.y},
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &after, second, 32, err, sizeof(err)));
    ASSERT_EQ(before.count, after.count);
    qsort(first, before.count, sizeof(*first), compare_pair);
    qsort(second, after.count, sizeof(*second), compare_pair);
    for (uint32_t i = 0; i < before.count; ++i) {
        ASSERT_INT_EQ(compare_pair(&first[i], &second[i]), 0);
        ASSERT_EQ(second[i].chunk.x, origin.x + 1);
        ASSERT_EQ(second[i].chunk.y, origin.y);
    }
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 99, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(77, 1, 20, 8), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){origin.x + 1, origin.y},
                                      err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &after, second, 32, err, sizeof(err)));
    ASSERT_EQ(body_pairs(second, after.count), 1u);
    for (uint32_t i = 0; i < after.count; ++i)
        ASSERT_TRUE(second[i].body_a != 99 && second[i].body_b != 99);
    dc_gpu_destroy(gpu); PASS();
}

static void test_missing_page_emits_world_boundary_until_resident(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(7, 63, 20, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[16];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    bool boundary = false;
    for (uint32_t i = 0; i < stats.count; ++i)
        boundary |= pairs[i].kind == DC_GPU_PAIR_BOUNDARY && pairs[i].body_a == 7 &&
                    pairs[i].chunk.x == 1 && pairs[i].chunk.y == 0;
    ASSERT_TRUE(boundary);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    bool terrain = false;
    for (uint32_t i = 0; i < stats.count; ++i) {
        ASSERT_TRUE(pairs[i].kind != DC_GPU_PAIR_BOUNDARY);
        terrain |= pairs[i].kind == DC_GPU_PAIR_TERRAIN && pairs[i].body_a == 7 &&
                   pairs[i].chunk.x == 1 && pairs[i].chunk.y == 0;
    }
    ASSERT_TRUE(terrain);
    dc_gpu_destroy(gpu); PASS();
}

static void test_capacity_overflow_is_explicit_and_reset_without_stale_pairs(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    for (uint32_t id = 1; id <= 5; ++id)
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(id, 20, 20, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_broadphase_capacity(gpu, 3, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[32];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 32, err, sizeof(err)));
    ASSERT_EQ(stats.capacity, 3u); ASSERT_EQ(stats.count, 3u);
    ASSERT_TRUE(stats.required >= 15u && stats.overflow != 0u);
    ASSERT_TRUE(dc_gpu_set_broadphase_capacity(gpu, DC_GPU_BROADPHASE_PAIR_CAPACITY,
                                             err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 32, err, sizeof(err)));
    ASSERT_EQ(stats.overflow, 0u); ASSERT_EQ(stats.count, stats.required);
    ASSERT_EQ(body_pairs(pairs, stats.count), 10u);
    dc_gpu_broadphase_stats_t sentinel = {.required = 999};
    ASSERT_TRUE(!dc_gpu_read_broadphase(gpu, &sentinel, pairs, 2, err, sizeof(err)));
    ASSERT_EQ(sentinel.required, 999u);
    for (uint32_t id = 1; id <= 5; ++id)
        ASSERT_TRUE(dc_gpu_remove_body(gpu, id, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u); ASSERT_EQ(stats.required, 0u);
    ASSERT_EQ(stats.overflow, 0u); ASSERT_EQ(stats.active_bodies, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_outer_neighbor_chunks_have_signed_boundary_keys(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = grid(err, sizeof(err));
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, box(8, 0, 0, 1), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_broadphase_stats_t stats;
    dc_gpu_broadphase_pair_t pairs[16];
    ASSERT_TRUE(dc_gpu_read_broadphase(gpu, &stats, pairs, 16, err, sizeof(err)));
    bool corner = false;
    for (uint32_t i = 0; i < stats.count; ++i)
        corner |= pairs[i].kind == DC_GPU_PAIR_BOUNDARY &&
                  pairs[i].chunk.x == -1 && pairs[i].chunk.y == -1;
    ASSERT_TRUE(corner);
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_dense_pool_reports_every_unique_body_pair);
    RUN(test_supported_fast_sweep_is_deduplicated_across_chunk_seam);
    RUN(test_crossing_paths_use_swept_bounds_and_expire_next_tick);
    RUN(test_nonoverlapping_bodies_in_one_bucket_do_not_form_pair);
    RUN(test_world_pair_keys_survive_camera_and_terrain_slot_reuse);
    RUN(test_missing_page_emits_world_boundary_until_resident);
    RUN(test_capacity_overflow_is_explicit_and_reset_without_stale_pairs);
    RUN(test_outer_neighbor_chunks_have_signed_boundary_keys);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
