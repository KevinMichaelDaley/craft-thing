#include <math.h>
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
    char err[256]; dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return NULL;
    bool okay = dc_gpu_create(&gpu, 128, 128, "build/shaders/pattern.comp.spv", err, sizeof(err));
    for (uint32_t slot = 0; okay && slot < 4; ++slot)
        okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)) &&
               dc_gpu_set_page(gpu, slot % 2, slot / 2, slot, err, sizeof(err));
    free(chunk);
    if (!okay) { dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_world_body_t box(uint32_t id, uint32_t x, uint32_t y, uint32_t side) {
    return (dc_gpu_world_body_t){ .chunk = {x / 64, y / 64}, .body = {
        .x_fp = (int32_t)(x % 64) << 16, .y_fp = (int32_t)(y % 64) << 16,
        .width = side, .height = side, .id = id, .active = 1 } };
}

static dc_gpu_body_shape_t diamond(uint32_t material) {
    return (dc_gpu_body_shape_t){ .count = 4, .material = material,
        .vertices = {{2 << 16, 0}, {4 << 16, 2 << 16},
                     {2 << 16, 4 << 16}, {0, 2 << 16}} };
}

static bool valid(const dc_gpu_contact_t *c) {
    float length = c->normal_x * c->normal_x + c->normal_y * c->normal_y;
    return c->body_a && isfinite(c->depth) && c->depth >= 0 &&
        fabsf(length - 1) < 0.0001f && c->friction > 0 && c->friction <= 1 &&
        isfinite(c->compliance) && c->compliance >= 0 &&
        c->anchor_a.x_fp >= 0 && c->anchor_a.x_fp < (64 << 16) &&
        c->anchor_a.y_fp >= 0 && c->anchor_a.y_fp < (64 << 16) &&
        c->anchor_b.x_fp >= 0 && c->anchor_b.x_fp < (64 << 16) &&
        c->anchor_b.y_fp >= 0 && c->anchor_b.y_fp < (64 << 16);
}

static void test_rotated_stone_and_wood_find_single_cell_steps_and_slopes(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    const uint32_t pixels[4][2] = {{63, 63}, {64, 64}, {65, 64}, {64, 65}};
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_TRUE(dc_gpu_paint_material(gpu, pixels[i][0], pixels[i][1], 0,
                                         DC_MATERIAL_STONE, err, sizeof(err)));
    for (uint32_t material = DC_GPU_BODY_STONE; material <= DC_GPU_BODY_WOOD; ++material) {
        dc_gpu_body_shape_t shape = diamond(material);
        ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, box(7, 62, 62, 4), &shape, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
        dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[64];
        ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 64, err, sizeof(err)));
        ASSERT_EQ(stats.overflow, 0u); ASSERT_EQ(stats.count, 4u);
        bool seen[4] = {false};
        for (uint32_t i = 0; i < stats.count; ++i) {
            const dc_gpu_contact_t *c = &contacts[i]; ASSERT_TRUE(valid(c));
            ASSERT_EQ(c->body_a, 7u); ASSERT_EQ(c->body_b, 0u);
            ASSERT_EQ(c->kind, DC_GPU_CONTACT_TERRAIN);
            ASSERT_EQ(c->material_a, material); ASSERT_EQ(c->material_b, DC_MATERIAL_STONE);
            bool found = false;
            for (uint32_t j = 0; j < 4; ++j) {
                if (c->feature_chunk.x != pixels[j][0] / 64 ||
                    c->feature_chunk.y != pixels[j][1] / 64 ||
                    c->feature_b != (pixels[j][1] % 64) * 64 + pixels[j][0] % 64) continue;
                ASSERT_TRUE(!seen[j]); seen[j] = true; found = true;
            }
            ASSERT_TRUE(found);
        }
        for (uint32_t i = 0; i < 4; ++i) ASSERT_TRUE(seen[i]);
    }
    dc_gpu_destroy(gpu); PASS();
}

static void test_empty_polygon_corners_and_water_produce_no_solid_contacts(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 65, 65, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 62, 62, 0, DC_MATERIAL_WATER, err, sizeof(err)));
    dc_gpu_body_shape_t triangle = { .count = 3, .material = DC_GPU_BODY_STONE,
        .vertices = {{0, 0}, {4 << 16, 0}, {0, 4 << 16}} };
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, box(11, 62, 62, 4), &triangle, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(12, 65, 65, 1), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_TRUE(stats.candidates > 0);
    for (uint32_t i = 0; i < stats.count; ++i) ASSERT_TRUE(contacts[i].body_a != 11u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_mpm_material_contacts_use_occupied_cells_and_distinct_kind(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    const uint32_t materials[3] = {DC_MATERIAL_SAND, DC_MATERIAL_DIRT, DC_MATERIAL_GRAVEL};
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_TRUE(dc_gpu_paint_material(gpu, 63 + i, 63, 0, materials[i], err, sizeof(err)));
    dc_gpu_body_shape_t shape = diamond(DC_GPU_BODY_WOOD);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, box(19, 62, 62, 4), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_EQ(stats.count, 3u); ASSERT_EQ(stats.overflow, 0u);
    bool seen[3] = {false};
    for (uint32_t i = 0; i < stats.count; ++i) {
        ASSERT_TRUE(valid(&contacts[i])); ASSERT_EQ(contacts[i].kind, DC_GPU_CONTACT_MPM);
        for (uint32_t j = 0; j < 3; ++j) if (contacts[i].material_b == materials[j]) seen[j] = true;
    }
    ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
    dc_gpu_destroy(gpu); PASS();
}

static void test_body_pair_is_symmetric_under_geometry_exchange(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_gpu_contact_t first = {0};
    for (uint32_t swap = 0; swap < 2; ++swap) {
        ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(swap ? 9 : 3, 62, 60, 4), err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(swap ? 3 : 9, 65, 60, 4), err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
        dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[16];
        ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 16, err, sizeof(err)));
        ASSERT_EQ(stats.count, 1u); ASSERT_TRUE(valid(&contacts[0]));
        ASSERT_EQ(contacts[0].body_a, 3u); ASSERT_EQ(contacts[0].body_b, 9u);
        ASSERT_EQ(contacts[0].kind, DC_GPU_CONTACT_BODY);
        ASSERT_TRUE(fabsf(contacts[0].depth - 1) < 0.0001f);
        if (!swap) { first = contacts[0]; ASSERT_TRUE(first.normal_x < -0.99f); }
        else {
            ASSERT_TRUE(contacts[0].normal_x > 0.99f);
            ASSERT_EQ(memcmp(&first.anchor_a, &contacts[0].anchor_b, sizeof(first.anchor_a)), 0);
            ASSERT_EQ(memcmp(&first.anchor_b, &contacts[0].anchor_a, sizeof(first.anchor_b)), 0);
        }
    }
    dc_gpu_destroy(gpu); PASS();
}

static int compare_contact(const void *left, const void *right) {
    const dc_gpu_contact_t *a = left, *b = right;
#define CMP(field) do { if (a->field != b->field) return a->field < b->field ? -1 : 1; } while (0)
    CMP(kind); CMP(body_a); CMP(body_b); CMP(feature_chunk.x); CMP(feature_chunk.y); CMP(feature_b); CMP(feature_a);
#undef CMP
    return 0;
}

static void test_world_anchors_and_features_survive_rebase_and_slot_reuse(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    dc_chunk_coord_t origin = {(INT64_C(1) << 40) - 1, -(INT64_C(1) << 40)};
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 95, 63, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    dc_gpu_world_body_t a = box(23, 94, 62, 4), b = box(29, 97, 62, 4);
    a.chunk.x += origin.x; a.chunk.y += origin.y;
    b.chunk.x += origin.x; b.chunk.y += origin.y;
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, origin, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t before[16], after[16];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, before, 16, err, sizeof(err)));
    uint32_t count = stats.count; ASSERT_EQ(count, 2u);
    qsort(before, count, sizeof(*before), compare_contact);
    ASSERT_TRUE(before[0].anchor_a.chunk.x >= origin.x);
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 23, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 29, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 1, 3, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 1, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){origin.x + 1, origin.y}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, after, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, count); ASSERT_EQ(stats.overflow, 0u);
    qsort(after, count, sizeof(*after), compare_contact);
    ASSERT_EQ(memcmp(before, after, count * sizeof(*before)), 0);
    dc_gpu_destroy(gpu); PASS();
}

static void test_bounded_overflow_and_incomplete_broadphase_gate_contacts(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    for (uint32_t i = 1; i <= 8; ++i)
        ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(i, 20, 20, 4), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_contact_capacity(gpu, 3, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_EQ(stats.count, 3u); ASSERT_EQ(stats.required, 28u);
    ASSERT_EQ(stats.overflow, DC_GPU_CONTACT_OVERFLOW_CAPACITY);
    ASSERT_TRUE(dc_gpu_set_contact_capacity(gpu, DC_GPU_CONTACT_CAPACITY, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_broadphase_capacity(gpu, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u); ASSERT_EQ(stats.overflow, DC_GPU_CONTACT_OVERFLOW_BROADPHASE);
    ASSERT_TRUE(dc_gpu_set_broadphase_capacity(gpu, DC_GPU_BROADPHASE_PAIR_CAPACITY, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_EQ(stats.count, 28u); ASSERT_EQ(stats.overflow, 0u);
    dc_gpu_contact_stats_t sentinel = {.count = 999}; dc_gpu_contact_t untouched = {.body_a = 999};
    ASSERT_TRUE(!dc_gpu_read_contacts(gpu, &sentinel, &untouched, 1, err, sizeof(err)));
    ASSERT_EQ(sentinel.count, 999u); ASSERT_EQ(untouched.body_a, 999u);
    for (uint32_t i = 1; i <= 8; ++i) ASSERT_TRUE(dc_gpu_remove_body(gpu, i, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u); ASSERT_EQ(stats.required, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_missing_chunk_boundary_produces_world_contacts(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(37, 63, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_TRUE(stats.count > 0); ASSERT_EQ(stats.overflow, 0u);
    for (uint32_t i = 0; i < stats.count; ++i) {
        ASSERT_TRUE(valid(&contacts[i])); ASSERT_EQ(contacts[i].kind, DC_GPU_CONTACT_BOUNDARY);
        ASSERT_EQ(contacts[i].feature_chunk.x, 1);
    }
    dc_gpu_destroy(gpu); PASS();
}

static void test_thin_polygon_edge_finds_one_cell_obstacle(void) {
    char err[256]; dc_gpu_t *gpu = grid(); ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 65, 10, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    dc_gpu_body_shape_t shape = { .count = 3, .material = DC_GPU_BODY_STONE,
        .vertices = {{0, 0}, {4 << 16, 0}, {0, 1 << 14}} };
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu, box(43, 62, 10, 4), &shape, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[16];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 16, err, sizeof(err)));
    ASSERT_EQ(stats.count, 1u); ASSERT_TRUE(valid(&contacts[0]));
    ASSERT_EQ(contacts[0].feature_chunk.x, 1); ASSERT_EQ(contacts[0].feature_b, 10u * 64u + 1u);
    ASSERT_TRUE(contacts[0].depth > 0 && contacts[0].depth < 0.25f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_partial_resident_tile_has_blocking_viewport_boundary(void) {
    char err[256]; dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 65, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    for (uint32_t slot = 0; slot < 2; ++slot) {
        ASSERT_TRUE(dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu, slot, 0, slot, err, sizeof(err)));
    }
    free(chunk);
    ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, box(47, 64, 10, 2), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, sizeof(err)));
    dc_gpu_contact_stats_t stats; dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(gpu, &stats, contacts, 32, err, sizeof(err)));
    ASSERT_TRUE(stats.count > 0); ASSERT_EQ(stats.overflow, 0u);
    for (uint32_t i = 0; i < stats.count; ++i) {
        ASSERT_TRUE(valid(&contacts[i])); ASSERT_EQ(contacts[i].kind, DC_GPU_CONTACT_BOUNDARY);
        ASSERT_EQ(contacts[i].feature_chunk.x, 1); ASSERT_TRUE(contacts[i].feature_b % 64u >= 1);
    }
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_partial_resident_tile_has_blocking_viewport_boundary);
    RUN(test_thin_polygon_edge_finds_one_cell_obstacle);
    RUN(test_rotated_stone_and_wood_find_single_cell_steps_and_slopes);
    RUN(test_empty_polygon_corners_and_water_produce_no_solid_contacts);
    RUN(test_mpm_material_contacts_use_occupied_cells_and_distinct_kind);
    RUN(test_body_pair_is_symmetric_under_geometry_exchange);
    RUN(test_world_anchors_and_features_survive_rebase_and_slot_reuse);
    RUN(test_bounded_overflow_and_incomplete_broadphase_gate_contacts);
    RUN(test_missing_chunk_boundary_produces_world_contacts);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
