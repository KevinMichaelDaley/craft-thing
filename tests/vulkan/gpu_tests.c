#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/gpu.h"

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); printf("OK   %s\n", #fn); } while (0)
#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); g_fail++; return; \
} } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_gpu_pattern_readback(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[0], 0xff202020u);
    ASSERT_EQ(cells[1], 0xff4040c0u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[15], 0xff202020u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_rejects_invalid_dimensions(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    ASSERT_TRUE(!dc_gpu_create(&gpu, 0, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(gpu == NULL);
    ASSERT_TRUE(strlen(err) > 0);
    PASS();
}

static void test_gpu_brush_updates_only_covered_cells(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    uint32_t cells[16] = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 4, 4, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_pattern(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint(gpu, 1, 1, 0, 0xff00ff00u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, cells, 16, err, sizeof(err)));
    ASSERT_EQ(cells[5], 0xff00ff00u);
    ASSERT_EQ(cells[4], 0xff4040c0u);
    ASSERT_EQ(cells[6], 0xff4040c0u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_chunk_page_mapping_and_gpu_material_edit(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0}, saved = {0};
    uint32_t pixels[128 * 64] = {0};
    left.coord = (dc_chunk_coord_t){-1, 0};
    right.coord = (dc_chunk_coord_t){0, 0};
    left.cells[5 * DC_CHUNK_SIDE + 63].material = DC_MATERIAL_STONE;
    right.cells[5 * DC_CHUNK_SIDE].fluid_mass = DC_FLUID_FULL;
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 128 * 64, err, sizeof(err)));
    ASSERT_EQ(pixels[5 * 128 + 63], 0xff707070u);
    ASSERT_EQ(pixels[5 * 128 + 64], 0xffd07030u);
    ASSERT_TRUE(dc_gpu_paint_material(gpu, 64, 5, 0, DC_MATERIAL_SAND, err, sizeof(err)));
    saved.coord = right.coord;
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 1, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE].material, DC_MATERIAL_SAND);
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE].fluid_mass, 0u);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, &saved, err, sizeof(err)));
    ASSERT_EQ(saved.cells[5 * DC_CHUNK_SIDE + 63].material, DC_MATERIAL_STONE);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_gpu_box_crosses_chunk_edge_and_rests_on_terrain(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0};
    uint32_t pixels[128 * 64] = {0};
    for (uint32_t x = 0; x < DC_CHUNK_SIDE; ++x) {
        left.cells[20 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
        right.cells[20 * DC_CHUNK_SIDE + x].material = DC_MATERIAL_STONE;
    }
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_body_t body = { .x_fp = 63 << 16, .y_fp = 1 << 16,
        .vx_fp = 1 << 16, .width = 2, .height = 2, .id = 1, .active = 1 };
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
    for (int i = 0; i < 20; ++i)
        ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.x_fp, 83 << 16);
    ASSERT_EQ(body.y_fp, 18 << 16);
    ASSERT_EQ(body.vy_fp, 0);
    ASSERT_TRUE(dc_gpu_render_chunks(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_readback(gpu, pixels, 128 * 64, err, sizeof(err)));
    ASSERT_EQ(pixels[18 * 128 + 83], 0xff30c040u);
    ASSERT_EQ(pixels[20 * 128 + 83], 0xff707070u);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_tick_capture_orders_gpu_stages_and_handoffs(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t left = {0}, right = {0};
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv", err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, &left, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 1, &right, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 1, 0, 1, err, sizeof(err)));
    dc_gpu_body_t body = { .x_fp = 63 << 16, .y_fp = 2 << 16,
        .vx_fp = 1 << 16, .width = 2, .height = 2, .id = 7, .active = 1 };
    ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
    dc_gpu_tick_capture_t capture = {0};
    ASSERT_TRUE(dc_gpu_tick_capture(gpu, &capture, err, sizeof(err)));
    ASSERT_EQ(capture.stages[0].id, DC_GPU_STAGE_RIGID);
    ASSERT_EQ(capture.stages[1].id, DC_GPU_STAGE_FLUID);
    ASSERT_EQ(capture.stages[2].id, DC_GPU_STAGE_SAND);
    ASSERT_EQ(capture.stages[0].handoff, 7u);
    ASSERT_EQ(capture.stages[1].handoff, 8u);
    ASSERT_EQ(capture.stages[2].handoff, 9u);
    ASSERT_TRUE(capture.stages[0].gpu_ns + capture.stages[1].gpu_ns +
                capture.stages[2].gpu_ns > 0);
    ASSERT_TRUE(dc_gpu_read_body(gpu, &body, err, sizeof(err)));
    ASSERT_EQ(body.x_fp, 64 << 16);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_gpu_pattern_readback);
    RUN(test_rejects_invalid_dimensions);
    RUN(test_gpu_brush_updates_only_covered_cells);
    RUN(test_chunk_page_mapping_and_gpu_material_edit);
    RUN(test_gpu_box_crosses_chunk_edge_and_rests_on_terrain);
    RUN(test_tick_capture_orders_gpu_stages_and_handoffs);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
