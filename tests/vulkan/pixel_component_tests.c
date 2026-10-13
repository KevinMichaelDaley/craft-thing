#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "dungeoncraft/gpu.h"
#include "ferrum/structural/gpu.h"
#include "../../src/vulkan/gpu_internal.h"
static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
static void scenario(unsigned kind) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    unsigned cells = 0;
    for (unsigned y = 10; y < 42; y++)
        for (unsigned x = 10; x < 50; x++) {
            bool fill = kind == 0   ? x < 30 && y < 30
                        : kind == 1 ? y == 10
                                    : ((x == 10 && y < 24) ||
                                       (y < 24 && y % 2 == 0 && x < 20));
            if (fill) {
                chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
                cells++;
            }
        }
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body;
    dc_gpu_body_motion_t motion;
    bool born = dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err));
    if (!born)
        printf("missing %u-pixel component: %s\n", cells, err);
    ASSERT_TRUE(born);
    ASSERT_TRUE(
        dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabs(motion.mass - cells * 2.7) < .002);
    ASSERT_TRUE(body.vy_fp > 0);
    dc_gpu_body_pixel_t pixels[400];
    uint32_t count = 0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, body.id, pixels, 400, &count,
                                       err, sizeof(err)));
    ASSERT_EQ(count, cells);
    for (unsigned i = 0; i < count; i++)
        ASSERT_EQ(pixels[i].material, DC_GPU_BODY_STONE);
    ASSERT_TRUE(fr_gpu_save_structure(gpu, "/tmp/ct-wuci-pixels.dcs", err,
                                      sizeof(err)));
    ASSERT_TRUE(fr_gpu_load_structure(gpu, "/tmp/ct-wuci-pixels.dcs", err,
                                      sizeof(err)));
    dc_gpu_body_pixel_t restored[400];
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, body.id, restored, 400, &count,
                                       err, sizeof(err)));
    for (unsigned i = 0; i < count; i++) {
        ASSERT_EQ(restored[i].x, pixels[i].x);
        ASSERT_EQ(restored[i].y, pixels[i].y);
        ASSERT_EQ(restored[i].material, pixels[i].material);
    }
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    for (unsigned i = 0; i < 4096; i++)
        ASSERT_EQ(chunk->cells[i].material, DC_MATERIAL_AIR);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_component_above_256_pixels_is_not_disqualified(void) {
    scenario(0);
}
static void test_long_thin_component_above_16_cells_is_not_disqualified(void) {
    scenario(1);
}
static void
test_branching_component_above_four_rectangles_is_not_disqualified(void) {
    scenario(2);
}
static void test_pixel_fracture_preserves_material_volume(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 10; y < 30; y++)
        for (unsigned x = 10; x < 30; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(gpu));
    uint32_t *pending = dc_gpu_structural_data(gpu) +
                        dc_gpu_structural_graph_offset() + 38000u;
    pending[0] = pending[1] = 1000000u;
    pending[2] = 1u;
    pending[3] = 0x80000001u;
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_motion_t a, b;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 0x80000001u, &a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 0x80000002u, &b, err, sizeof(err)));
    ASSERT_TRUE(fabs(a.mass + b.mass - 1080) < .002);
    uint32_t ca, cb;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, 0x80000001u, NULL, 0, &ca,
                                       err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_pixel_shape(gpu, 0x80000002u, NULL, 0, &cb,
                                       err, sizeof(err)));
    ASSERT_EQ(ca + cb, 400u);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_large_pixel_body_has_accurate_mass_center_and_inertia(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 1; y < 63; y++)
        for (unsigned x = 1; x < 63; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 0x80000001u, &motion, err, sizeof(err)));
    double mass = 62 * 62 * 2.7, inertia = mass * (62 * 62 + 62 * 62) / 12;
    printf("large mass %.9g inertia %.9g expected %.9g\n", motion.mass, motion.inertia, inertia);
    ASSERT_TRUE(fabs(motion.mass - mass) < mass * 1e-6);
    ASSERT_TRUE(fabs(motion.center_x - 31) < 1e-5);
    ASSERT_TRUE(fabs(motion.center_y - 31) < 1e-5);
    ASSERT_TRUE(fabs(motion.inertia - inertia) < inertia * 1e-6);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
static void test_pending_component_is_not_starved_by_new_lower_roots(void) {
    char err[256] = {0};
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 40; y++)
        for (unsigned x = 10; x < 60; x++)
            if (x < 30 || x >= 40)
                chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    for (unsigned i = 0; i < 64; i++) {
        dc_gpu_body_t body = {.x_fp = (int32_t)(1 + i % 8) << 16,
                              .y_fp = (int32_t)(1 + i / 8) << 16,
                              .width = 1, .height = 1, .id = 1000 + i,
                              .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(gpu, body, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_motion(gpu, body.id, 0, 0,
                                           DC_GPU_BODY_KINEMATIC, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 1063, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 0x80000001u, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t born;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 0x80000002u, &born, err, sizeof(err)));
    ASSERT_TRUE(born.x_fp > 35 * 65536);
    free(chunk);
    dc_gpu_destroy(gpu);
    PASS();
}
int main(void) {
    RUN(test_component_above_256_pixels_is_not_disqualified);
    RUN(test_long_thin_component_above_16_cells_is_not_disqualified);
    RUN(test_branching_component_above_four_rectangles_is_not_disqualified);
    RUN(test_pixel_fracture_preserves_material_volume);
    RUN(test_large_pixel_body_has_accurate_mass_center_and_inertia);
    RUN(test_pending_component_is_not_starved_by_new_lower_roots);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
