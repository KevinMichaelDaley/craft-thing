#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "dungeoncraft/gpu.h"
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
int main(void) {
    RUN(test_component_above_256_pixels_is_not_disqualified);
    RUN(test_long_thin_component_above_16_cells_is_not_disqualified);
    RUN(test_branching_component_above_four_rectangles_is_not_disqualified);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
