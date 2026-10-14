#include <stdio.h>
#include <string.h>

#include "ferrum/structural/authoring.h"

static int g_pass, g_fail;
_Static_assert(sizeof(fr_authored_provenance_t) == 16, "Provenance ABI");
_Static_assert(sizeof(fr_authored_destination_t) == 64, "Destination ABI");
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() g_pass++

static uint8_t physical[4 * FR_AUTHORED_PHYSICAL_STRIDE];
static uint8_t appearance[4 * FR_AUTHORED_APPEARANCE_STRIDE];

static fr_authored_request_t request(void) {
    return (fr_authored_request_t){
        .physical = physical, .appearance = appearance,
        .physical_bytes = sizeof(physical), .appearance_bytes = sizeof(appearance),
        .width = 2, .height = 2, .state_variant_count = 1,
        .asset_uuid = {1}, .transaction_id = 7,
        .origin = {INT64_MIN + 2, INT64_MAX - 2},
        .origin_x = 63, .origin_y = 0, .pivot_x = -7, .pivot_y = 81};
}

static void test_canonical_far_world_request_preserves_identity(void) {
    fr_authored_request_t r = request(), old = r;
    char err[128];
    ASSERT_TRUE(fr_authored_request_valid(&r, err, sizeof(err)));
    ASSERT_EQ(memcmp(&old, &r, sizeof(r)), 0);
    PASS();
}

static void test_explicit_state_variants_remain_separate_from_animation(void) {
    fr_authored_request_t r = request();
    r.state_variant_count = 3;
    r.state_variant = 2;
    r.source_frame = 18;
    ASSERT_TRUE(fr_authored_request_valid(&r, NULL, 0));
    r.state_variant = 3;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    PASS();
}

static void test_truncated_maps_and_zero_identity_rejected(void) {
    fr_authored_request_t r = request();
    r.physical_bytes--;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); r.appearance_bytes--;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); r.transaction_id = 0;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); memset(r.asset_uuid, 0, sizeof(r.asset_uuid));
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    PASS();
}

static void test_coordinate_overflow_and_ambiguous_member_plane_rejected(void) {
    fr_authored_request_t r = request();
    r.origin.x = INT64_MIN; r.origin_x = 0; r.pivot_x = 1;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); r.origin_x = 64;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); r.width = UINT32_MAX; r.height = UINT32_MAX;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request(); r.member_ownership = physical; r.member_ownership_bytes = 16;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    PASS();
}

static void test_bounded_graph_and_state_count_are_preflighted(void) {
    fr_authored_request_t r = request();
    fr_structural_gpu_graph_t graph = {0};
    uint8_t members[16] = {0};
    r.structure = &graph;
    r.member_ownership = members;
    r.member_ownership_bytes = sizeof(members);
    ASSERT_TRUE(fr_authored_request_valid(&r, NULL, 0));
    graph.node_count = FR_STRUCTURAL_NODE_CAPACITY + 1;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    r = request();
    r.state_variant_count = 4097;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    PASS();
}

static void test_exact_world_extents_allow_limits_without_wrap(void) {
    fr_authored_request_t r = request();
    r.origin = (dc_chunk_coord_t){INT64_MIN, INT64_MAX};
    r.origin_x = 0; r.origin_y = 62;
    r.pivot_x = 0; r.pivot_y = 0;
    ASSERT_TRUE(fr_authored_request_valid(&r, NULL, 0));
    r.origin_y = 63;
    ASSERT_TRUE(!fr_authored_request_valid(&r, NULL, 0));
    PASS();
}

int main(void) {
    RUN(test_canonical_far_world_request_preserves_identity);
    RUN(test_explicit_state_variants_remain_separate_from_animation);
    RUN(test_truncated_maps_and_zero_identity_rejected);
    RUN(test_coordinate_overflow_and_ambiguous_member_plane_rejected);
    RUN(test_bounded_graph_and_state_count_are_preflighted);
    RUN(test_exact_world_extents_allow_limits_without_wrap);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
