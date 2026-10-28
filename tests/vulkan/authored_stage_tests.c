#include <stdio.h>
#include <string.h>

#include "ferrum/structural/authoring.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a), (int)(b))
#define PASS() g_pass++

static uint8_t physical[8 * 8 * 8], appearance[8 * 8 * 64];

static fr_authored_request_t request(void) {
    memset(physical, 0, sizeof(physical));
    memset(appearance, 0, sizeof(appearance));
    for (unsigned i = 0; i < 64; i++) {
        float normal = 1;
        memcpy(appearance + i * 64 + 24, &normal, sizeof(normal));
    }
    return (fr_authored_request_t){
        .physical = physical, .appearance = appearance,
        .physical_bytes = sizeof(physical), .appearance_bytes = sizeof(appearance),
        .width = 8, .height = 8, .state_variant_count = 1,
        .asset_uuid = {1}, .transaction_id = 9};
}

static void pixel(unsigned x, unsigned y, unsigned material, unsigned role,
                   unsigned object) {
    unsigned i = (y * 8 + x) * 8;
    physical[i] = (uint8_t)material;
    physical[i + 2] = (uint8_t)role;
    physical[i + 4] = (uint8_t)object;
}

static dc_gpu_t *context(void) {
    char err[256];
    dc_gpu_t *gpu = NULL;
    if (!dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)))
        return NULL;
    return gpu;
}

static void test_mixed_concavities_keep_exact_role_object_connectivity(void) {
    fr_authored_request_t r = request();
    pixel(1,1,1,3,7); pixel(1,2,2,3,7); pixel(1,3,4,3,7); pixel(2,3,1,3,7);
    pixel(4,1,2,3,7); pixel(4,2,1,3,7); pixel(5,2,4,3,7);
    pixel(6,2,1,3,8);
    pixel(2,4,1,1,7);
    pixel(7,7,0,4,7);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu);
    char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    uint32_t labels[64];
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, labels, 64, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u); ASSERT_EQ(d.rigid_components, 3u);
    ASSERT_EQ(d.rigid_pixels, 8u); ASSERT_EQ(d.terrain_pixels, 1u);
    ASSERT_EQ(d.decoration_pixels, 1u);
    ASSERT_EQ(labels[9], labels[26]);
    ASSERT_TRUE(labels[9] != labels[12]);
    ASSERT_TRUE(labels[21] != labels[22]);
    ASSERT_EQ(labels[34], FR_AUTHORED_INVALID_OUTPUT);
    fr_authored_component_plan_t plans[3];
    uint32_t count = 0;
    ASSERT_TRUE(!fr_gpu_read_authored_components(gpu, plans, 2, &count, err, sizeof(err)));
    ASSERT_EQ(count, 3u);
    ASSERT_TRUE(fr_gpu_read_authored_components(gpu, plans, 3, &count, err, sizeof(err)));
    ASSERT_EQ(count, 3u); ASSERT_EQ(plans[0].source_root, 9u);
    ASSERT_EQ(plans[0].object, 7u); ASSERT_EQ(plans[0].occupied, 4u);
    ASSERT_EQ(plans[0].low_x, 1u); ASSERT_EQ(plans[0].low_y, 1u);
    ASSERT_EQ(plans[0].width, 2u); ASSERT_EQ(plans[0].height, 3u);
    ASSERT_EQ(plans[0].tiles, 1u); ASSERT_EQ(plans[0].geometry_words, 42u);
    ASSERT_EQ(plans[1].source_root, 12u); ASSERT_EQ(plans[2].source_root, 22u);
    ASSERT_EQ(plans[0].body_slot, 0u); ASSERT_EQ(plans[1].body_slot, 1u);
    ASSERT_EQ(plans[2].body_slot, 2u);
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.active, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_diagonal_pixels_remain_independent(void) {
    fr_authored_request_t r = request();
    pixel(1,1,2,3,0); pixel(2,2,2,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.rigid_components, 2u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_invalid_role_material_and_fabricated_structure_are_diagnosed(void) {
    fr_authored_request_t r = request(); pixel(3,3,5,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    ASSERT_EQ(d.first_invalid_pixel, 27u);
    r = request(); pixel(3,3,1,2,0); physical[27 * 8 + 3] = 1;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    dc_gpu_destroy(gpu); PASS();
}

static void test_staging_resize_releases_allocation_and_bad_envelope_keeps_prior(void) {
    fr_authored_request_t r = request(); pixel(1,1,2,3,0);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    dc_gpu_memory_stats_t large, small;
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &large));
    r.width = 1; r.height = 1;
    r.physical_bytes = 8; r.appearance_bytes = 64;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_memory_stats(gpu, &small));
    ASSERT_TRUE(small.mapped_local_bytes + small.mapped_system_bytes <
                large.mapped_local_bytes + large.mapped_system_bytes);
    r.width = UINT32_MAX;
    ASSERT_TRUE(!fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u); ASSERT_EQ(d.rigid_pixels, 0u);
    dc_gpu_destroy(gpu); PASS();
}

static void test_absent_normal_uses_renderer_default_not_raw_placeholder(void) {
    fr_authored_request_t r = request();
    memset(appearance + 16, 0, 12);
    dc_gpu_t *gpu = context(); ASSERT_TRUE(gpu); char err[256];
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    fr_authored_stage_diagnostic_t d;
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_EQ(d.failures, 0u);
    appearance[60] = 1;
    ASSERT_TRUE(fr_gpu_prepare_authored(gpu, &r, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_read_authored_stage(gpu, &d, NULL, 0, err, sizeof(err)));
    ASSERT_TRUE(d.failures & FR_AUTHORED_FAIL_INPUT);
    ASSERT_EQ(d.first_invalid_pixel, 0u);
    dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_mixed_concavities_keep_exact_role_object_connectivity);
    RUN(test_diagonal_pixels_remain_independent);
    RUN(test_invalid_role_material_and_fabricated_structure_are_diagnosed);
    RUN(test_staging_resize_releases_allocation_and_bad_envelope_keeps_prior);
    RUN(test_absent_normal_uses_renderer_default_not_raw_placeholder);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
