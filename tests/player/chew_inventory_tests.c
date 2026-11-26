#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/gpu.h"
#include "dungeoncraft/material_density.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/reactive_state.h"
#include "ferrum/player/chew.h"
#include "../../src/vulkan/gpu_internal.h"

static int g_pass, g_fail;
static void teardown(void);
#define RUN(fn) do {printf("RUN %s\n",#fn);fflush(stdout);fn();teardown();} while (0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() g_pass++

/* A wood wall in chunk (3,3) plus a dense wood pillar, both reactive. */
static dc_gpu_t *gpu = NULL;
static char err[256];

static bool seed_wood(uint32_t slot, uint32_t local, uint16_t material) {
    const fr_reactive_profile_t *profile = fr_reactive_profile(material);
    if (!profile) return false;
    uint32_t mass = (uint32_t)lround(profile->density * 65536.);
    fr_reactive_state_t state;
    static uint64_t parcel = 0x72000000u;
    if (!fr_reactive_state_default(material, mass, 0, 293.15f, ++parcel,
                                   &state, err, sizeof(err)))
        return false;
    return fr_gpu_reactive_seed_cell(gpu, slot, local, &state, err,
                                     sizeof(err));
}

static bool setup_world(void) {
    err[0] = 0;
    if (!dc_gpu_create(&gpu, 480, 270,
                       "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return false;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    chunk->coord = (dc_chunk_coord_t){3, 3};
    for (uint32_t x = 0; x < 64u; x++)
        chunk->cells[40u * 64u + x].material = DC_MATERIAL_STONE;
    for (uint32_t y = 20u; y < 40u; y++)
        chunk->cells[y * 64u + 30u].material = DC_MATERIAL_WOOD;
    for (uint32_t y = 20u; y < 40u; y++)
        chunk->cells[y * 64u + 40u].material = DC_MATERIAL_DENSE_WOOD;
    bool okay = dc_gpu_upload_chunk(gpu, 11, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 3, 3, 11, err, sizeof(err)) &&
                dc_gpu_enable_automatic_components(gpu) &&
                dc_gpu_tick_step(gpu, err, sizeof(err));
    free(chunk);
    if (!okay) return false;
    for (uint32_t y = 20u; y < 40u; y++)
        if (!seed_wood(11u, y * 64u + 30u, DC_MATERIAL_WOOD) ||
            !seed_wood(11u, y * 64u + 40u, DC_MATERIAL_DENSE_WOOD))
            return false;
    return true;
}

static fr_player_point_t target_at(float world_x, float world_y) {
    return (fr_player_point_t){.chunk = {3, 3},
        .x = world_x - 192.f, .y = world_y - 192.f};
}

static void teardown(void) {
    dc_gpu_destroy(gpu);
    gpu = NULL;
}

static void test_chew_credits_removed_wood_mass(void) {
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t inv;
    fr_player_inventory_init(&inv);
    fr_player_chew_t chew = {0};
    fr_player_point_t target = target_at(222.f, 214.f);
    uint32_t local = 30u + 22u * 64u;
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &target, 300.f,
                                    err, sizeof(err)));
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &target, 300.f,
                                    err, sizeof(err)));
    uint64_t wood = fr_player_inventory_mass(&inv, DC_ITEM_WOOD);
    printf("chew inventory wood %llu fp\n", (unsigned long long)wood);
    /* Each completed tick banks at most one bite: two bites, two cells. */
    ASSERT_TRUE(wood == 2u * (uint64_t)lround(DC_DENSITY_WOOD * 65536.));
    dc_chunk_t *probe = calloc(1, sizeof(*probe));
    ASSERT_TRUE(probe && dc_gpu_download_chunk(gpu, 11u, probe, err,
                                               sizeof(err)));
    ASSERT_EQ(probe->cells[local].material, DC_MATERIAL_AIR);
    free(probe);
    teardown();
    PASS();
}
static void test_dense_wood_takes_longer_per_cell(void) {
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t inv;
    fr_player_inventory_init(&inv);
    fr_player_chew_t chew = {0};
    /* One 1.0-second chewing window completes a wood bite (0.6 s at the
     * fixed rate) but a dense pocket needs 1.2 s, so it only banks. */
    fr_player_point_t target = target_at(232.f, 214.f);
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &target, 300.f,
                                    err, sizeof(err)));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD), 0u);
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_DENSE_WOOD), 0u);
    /* The dense pocket banked real work toward its first bite. */
    ASSERT_TRUE(chew.work > 0u);
    ASSERT_EQ(chew.material, DC_MATERIAL_DENSE_WOOD);
    teardown();
    PASS();
}
static void test_effort_carries_across_ticks_and_cadences(void) {
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t one;
    fr_player_inventory_init(&one);
    fr_player_chew_t chew = {0};
    fr_player_point_t target = target_at(222.f, 214.f);
    /* Ten 30-effort ticks. */
    for (uint32_t tick = 0u; tick < 10u; tick++)
        ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &one, &chew, &target, 30.f,
                                        err, sizeof(err)));
    uint64_t slow = fr_player_inventory_mass(&one, DC_ITEM_WOOD);
    teardown();
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t two;
    fr_player_inventory_init(&two);
    fr_player_chew_t chew2 = {0};
    target = target_at(222.f, 230.f);
    /* The same 300 total effort in one tick. */
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &two, &chew2, &target, 300.f,
                                    err, sizeof(err)));
    uint64_t fast = fr_player_inventory_mass(&two, DC_ITEM_WOOD);
    printf("cadence wood slow %llu fast %llu\n",
           (unsigned long long)slow, (unsigned long long)fast);
    /* Equal elapsed effort removes equal mass across cadences. */
    ASSERT_EQ(slow, fast);
    teardown();
    PASS();
}
static void test_invalid_targets_never_credit(void) {
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t inv;
    fr_player_inventory_init(&inv);
    fr_player_chew_t chew = {0};
    /* Air, stone floor, and empty grid corners hold nothing to chew. */
    fr_player_point_t air = target_at(222.f, 233.f);
    fr_player_point_t stone = target_at(240.f, 240.f);
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &air, 300.f, err,
                                    sizeof(err)));
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &stone, 300.f,
                                    err, sizeof(err)));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD), 0u);
    /* Nothing banked: invalid targets are transactional no-ops. */
    ASSERT_EQ(chew.work, 0u);
    teardown();
    PASS();
}
static void test_chewing_a_support_releases_detached_wood(void) {
    ASSERT_TRUE(setup_world());
    fr_player_inventory_t inv;
    fr_player_inventory_init(&inv);
    fr_player_chew_t chew = {0};
    /* Cut the pillar at row 30; the trunk above detaches as rigid wood. */
    for (uint32_t bite = 0u; bite < 120u; bite++) {
        fr_player_point_t bite_target = target_at(222.f, 222.f);
        ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew,
                                        &bite_target, 300.f, err,
                                        sizeof(err)));
        ASSERT_TRUE(dc_gpu_tick_step(gpu, err, sizeof(err)));
    }
    uint64_t wood = fr_player_inventory_mass(&inv, DC_ITEM_WOOD);
    printf("sustained chew wood %llu fp\n", (unsigned long long)wood);
    /* Many bites consumed real cells, and the released wood became bodies. */
    ASSERT_TRUE(wood > 0u);
    uint32_t bodies = 0u;
    for (uint32_t k = 0u; k < 64u; k++) {
        dc_gpu_world_body_t body;
        if (dc_gpu_read_world_body(gpu, 0x80000001u + k, &body, err,
                                   sizeof(err)) &&
            body.body.active)
            bodies++;
    }
    printf("rigid pieces after the cut %u\n", bodies);
    ASSERT_TRUE(bodies > 0u);
    teardown();
    PASS();
}

static bool map_cell(uint32_t slot, dc_chunk_coord_t coord, uint32_t page_x,
                     uint32_t page_y, uint32_t local, uint16_t material) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return false;
    chunk->coord = coord;
    chunk->cells[local].material = material;
    bool okay = dc_gpu_upload_chunk(gpu, slot, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, page_x, page_y, slot, err, sizeof(err));
    free(chunk);
    return okay;
}

static uint16_t cell_material(uint32_t slot, uint32_t local) {
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk) return UINT16_MAX;
    bool okay = dc_gpu_download_chunk(gpu, slot, chunk, err, sizeof(err));
    uint16_t material = okay ? chunk->cells[local].material : UINT16_MAX;
    free(chunk);
    return material;
}

static void test_matching_local_cell_in_another_chunk_is_not_harvested(void) {
    ASSERT_TRUE(setup_world());
    const uint32_t cell = 20u + 20u * 64u;
    ASSERT_TRUE(map_cell(1u, (dc_chunk_coord_t){2, 3}, 2u, 3u, cell, DC_MATERIAL_WOOD));
    fr_player_inventory_t inv = {0};
    fr_player_chew_t chew = {0};
    fr_player_point_t aim = {{3, 3}, 20.f, 20.f};
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &aim, 300.f,
                                        err, sizeof(err)));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD), 0u);
    ASSERT_EQ(cell_material(1u, cell), DC_MATERIAL_WOOD);
    PASS();
}

static void test_harvest_remapped_stone_and_credit_only_its_owner(void) {
    ASSERT_TRUE(setup_world());
    const uint32_t cell = 20u + 20u * 64u;
    fr_player_point_t aim = {{-30, 112}, 20.f, 20.f};
    ASSERT_TRUE(map_cell(1u, aim.chunk, 2u, 2u, cell, DC_MATERIAL_STONE));
    fr_player_inventory_t inv = {0}, other = {0};
    fr_player_chew_t chew = {0};
    for (uint32_t tick = 0u; tick < 4u; ++tick)
        ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &aim, 300.f,
                                            err, sizeof(err)));
    ASSERT_INT_EQ(cell_material(1u, cell), DC_MATERIAL_AIR);
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_STONE),
              (uint64_t)lround(DC_DENSITY_STONE * 65536.));
    ASSERT_EQ(fr_player_inventory_mass(&inv, DC_ITEM_WOOD), 0u);
    ASSERT_EQ(fr_player_inventory_mass(&other, DC_ITEM_STONE), 0u);
    PASS();
}

static void test_full_inventory_preserves_cell_for_retry(void) {
    ASSERT_TRUE(setup_world());
    const uint32_t cell = 30u + 22u * 64u;
    const uint64_t mass = (uint64_t)lround(DC_DENSITY_WOOD * 65536.);
    fr_player_inventory_t inv = {0};
    inv.mass_fp[DC_ITEM_WOOD] = UINT64_MAX - mass + 1u;
    fr_player_chew_t chew = {0};
    fr_player_point_t aim = {{3, 3}, 30.f, 22.f};
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &aim, 300.f,
                                        err, sizeof(err)));
    ASSERT_EQ(cell_material(11u, cell), DC_MATERIAL_WOOD);
    ASSERT_EQ(inv.mass_fp[DC_ITEM_WOOD], UINT64_MAX - mass + 1u);
    ASSERT_TRUE(chew.armed);
    inv.mass_fp[DC_ITEM_WOOD] = 0u;
    ASSERT_TRUE(fr_player_chew_wood_tick(gpu, &inv, &chew, &aim, 1.f,
                                        err, sizeof(err)));
    ASSERT_EQ(cell_material(11u, cell), DC_MATERIAL_AIR);
    ASSERT_EQ(inv.mass_fp[DC_ITEM_WOOD], mass);
    ASSERT_TRUE(!chew.armed);
    PASS();
}

int main(void) {
    printf("=== Player Chew Inventory Tests ===\n\n");
    RUN(test_chew_credits_removed_wood_mass);
    RUN(test_dense_wood_takes_longer_per_cell);
    RUN(test_effort_carries_across_ticks_and_cadences);
    RUN(test_invalid_targets_never_credit);
    RUN(test_chewing_a_support_releases_detached_wood);
    RUN(test_matching_local_cell_in_another_chunk_is_not_harvested);
    RUN(test_harvest_remapped_stone_and_credit_only_its_owner);
    RUN(test_full_inventory_preserves_cell_for_retry);
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
