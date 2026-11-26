#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/player/abilities.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static dc_gpu_t gpu;
static uint32_t pages[9], brush_calls, legacy_paint_calls, read_mask;
static char err[128];

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); free(gpu.body_mapped); gpu.body_mapped = NULL; } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #expr, err); ++g_fail; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() ++g_pass

static bool setup(void) {
    memset(pages, 0, sizeof(pages));
    brush_calls = legacy_paint_calls = read_mask = 0u;
    err[0] = '\0';
    gpu = (dc_gpu_t){.width = 192u, .height = 192u, .page_width = 3u, .page_height = 3u,
        .page_mapped = pages, .runtime_pair_capacity = 1u, .runtime_contact_capacity = 1u};
    for (uint32_t slot = 0; slot < DC_GPU_CHUNK_SLOTS; ++slot) gpu.slot_page[slot] = UINT32_MAX;
    size_t words = dc_gpu_structural_word_offset(&gpu) + dc_gpu_structural_graph_offset();
    gpu.body_mapped = calloc(1, DC_GPU_BODY_CAPACITY * sizeof(dc_gpu_body_record_t) + words * sizeof(uint32_t));
    return gpu.body_mapped != NULL;
}

static void map(uint32_t page, uint32_t slot, dc_chunk_coord_t coord) {
    pages[page] = slot + 1u;
    gpu.slot_page[slot] = page;
    dc_gpu_structural_track_chunk(&gpu, slot, coord);
}

bool dc_gpu_paint_material(dc_gpu_t *world, uint32_t x, uint32_t y, uint32_t radius,
                           uint16_t material, char *error, uint32_t cap) {
    (void)x; (void)y; (void)radius; (void)error; (void)cap;
    ++legacy_paint_calls;
    return world == &gpu && material == DC_MATERIAL_WATER;
}

bool dc_gpu_paint_chilled_water(dc_gpu_t *world, uint32_t x, uint32_t y, uint32_t radius,
                                char *error, uint32_t cap) {
    (void)x; (void)y; (void)radius; (void)error; (void)cap;
    ++brush_calls;
    return world == &gpu;
}

bool fr_gpu_reactive_read_cell(dc_gpu_t *world, uint32_t slot, uint32_t local_cell,
    fr_reactive_record_t *out, bool *found, char *error, uint32_t cap) {
    (void)world; (void)local_cell; (void)out; (void)error; (void)cap;
    read_mask |= 1u << slot;
    *found = false;
    return true;
}

bool fr_gpu_reactive_seed_cell(dc_gpu_t *world, uint32_t slot, uint32_t local_cell,
    const fr_reactive_state_t *state, char *error, uint32_t cap) {
    (void)world; (void)slot; (void)local_cell; (void)state; (void)error; (void)cap;
    return false;
}

static const fr_player_point_t seam = {{-20, 118}, 63.f, 12.f};

static void test_wrong_world_page_rejects_the_entire_storm(void) {
    ASSERT_TRUE(setup());
    map(4u, 0u, seam.chunk);
    map(5u, 1u, (dc_chunk_coord_t){100, -100});
    bool applied = fr_player_cast_ice_storm(&gpu, &seam, 4u, err, sizeof(err));
    printf("Mismatched seam: admitted=%d brush calls=%u thermal pages=%#x\n",
           applied, brush_calls + legacy_paint_calls, read_mask);
    ASSERT_EQ(brush_calls + legacy_paint_calls, 0u);
    ASSERT_EQ(read_mask, 0u);
    ASSERT_TRUE(!applied);
    PASS();
}

static void test_neighbor_resident_at_the_wrong_physical_page_rejects_storm(void) {
    ASSERT_TRUE(setup());
    map(4u, 0u, seam.chunk);
    map(8u, 1u, (dc_chunk_coord_t){-19, 118});
    ASSERT_TRUE(!fr_player_cast_ice_storm(&gpu, &seam, 4u, err, sizeof(err)));
    ASSERT_EQ(brush_calls + legacy_paint_calls, 0u);
    PASS();
}

static void test_true_negative_seam_is_applied_and_unmapped_seam_is_skipped(void) {
    ASSERT_TRUE(setup());
    map(4u, 0u, seam.chunk);
    ASSERT_TRUE(fr_player_cast_ice_storm(&gpu, &seam, 4u, err, sizeof(err)));
    printf("Mapped storm: device brushes=%u legacy brushes=%u parcel-read mask=%#x\n",
           brush_calls, legacy_paint_calls, read_mask);
    ASSERT_EQ(brush_calls, 1u);
    ASSERT_EQ(legacy_paint_calls, 0u);
    ASSERT_EQ(read_mask, 0u);
    map(5u, 1u, (dc_chunk_coord_t){-19, 118});
    ASSERT_TRUE(fr_player_cast_ice_storm(&gpu, &seam, 4u, err, sizeof(err)));
    ASSERT_EQ(brush_calls, 2u);
    ASSERT_EQ(legacy_paint_calls, 0u);
    ASSERT_EQ(read_mask, 0u);
    PASS();
}

static void test_bad_diagonal_outside_the_disc_does_not_block_it(void) {
    ASSERT_TRUE(setup());
    const fr_player_point_t corner = {{-20, 118}, 61.f, 61.f};
    map(4u, 0u, corner.chunk);
    map(5u, 1u, (dc_chunk_coord_t){-19, 118});
    map(7u, 2u, (dc_chunk_coord_t){-20, 119});
    map(8u, 3u, (dc_chunk_coord_t){100, -100});
    ASSERT_TRUE(fr_player_cast_ice_storm(&gpu, &corner, 3u, err, sizeof(err)));
    ASSERT_EQ(brush_calls, 1u);
    ASSERT_EQ(legacy_paint_calls, 0u);
    ASSERT_EQ(read_mask, 0u);
    PASS();
}

static void test_footprint_cannot_cross_the_world_coordinate_limit(void) {
    ASSERT_TRUE(setup());
    const fr_player_point_t extreme = {{INT64_MIN, INT64_MAX}, 0.f, 63.f};
    map(4u, 0u, extreme.chunk);
    ASSERT_TRUE(!fr_player_cast_ice_storm(&gpu, &extreme, 4u, err, sizeof(err)));
    ASSERT_EQ(brush_calls + legacy_paint_calls, 0u);
    PASS();
}

int main(void) {
    RUN(test_wrong_world_page_rejects_the_entire_storm);
    RUN(test_neighbor_resident_at_the_wrong_physical_page_rejects_storm);
    RUN(test_true_negative_seam_is_applied_and_unmapped_seam_is_skipped);
    RUN(test_bad_diagonal_outside_the_disc_does_not_block_it);
    RUN(test_footprint_cannot_cross_the_world_coordinate_limit);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
