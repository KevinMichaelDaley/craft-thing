#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"
#include "ferrum/structural/gpu.h"

static int g_pass, g_fail;
#define RUN(fn)                                                                \
    do {                                                                       \
        printf("RUN %s\n", #fn);                                               \
        fn();                                                                  \
    } while (0)
#define ASSERT_TRUE(e)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e);                \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_wet_mixed_component_crosses_a_warm_chunk_seam(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunks = calloc(2, sizeof(*chunks));
    ASSERT_TRUE(chunks);
    chunks[1].coord.x = 1;
    for (unsigned y = 20; y < 24; y++)
        for (unsigned x = 62; x < 66; x++) {
            dc_cell_t *cell = &chunks[x / 64].cells[y * 64 + x % 64];
            cell->material = x < 64 ? DC_MATERIAL_WOOD : DC_MATERIAL_STONE;
            cell->fluid_mass = DC_FLUID_FULL;
        }
    ASSERT_TRUE(dc_gpu_create(&gpu, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(
            dc_gpu_upload_chunk(gpu, slot, &chunks[slot], err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(gpu, slot, 0, slot, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    dc_gpu_body_t body;
    bool extracted =
        dc_gpu_read_body_id(gpu, 0x80000001u, &body, err, sizeof(err)) &&
        dc_gpu_read_body_motion(gpu, body.id, &motion, err, sizeof(err));
    unsigned material_cells = 0;
    uint64_t water = 0;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(
            dc_gpu_download_chunk(gpu, slot, &chunks[slot], err, sizeof(err)));
        for (unsigned cell = 0; cell < 4096; cell++) {
            material_cells +=
                chunks[slot].cells[cell].material != DC_MATERIAL_AIR;
            water += chunks[slot].cells[cell].fluid_mass;
        }
    }
    ASSERT_TRUE(fr_gpu_save_structure(gpu, "build/extracted_seam.dcs", err,
                                      sizeof(err)));
    dc_gpu_t *copy = NULL;
    ASSERT_TRUE(dc_gpu_create(&copy, 128, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_upload_chunk(copy, slot, &chunks[slot], err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(copy, slot, 0, slot, err, sizeof(err)));
    }
    ASSERT_TRUE(fr_gpu_load_structure(copy, "build/extracted_seam.dcs", err,
                                      sizeof(err)));
    dc_gpu_compound_shape_t shape;
    ASSERT_TRUE(
        dc_gpu_read_compound_shape(copy, body.id, &shape, err, sizeof(err)));
    ASSERT_EQ(shape.pieces[0].material, DC_GPU_BODY_WOOD);
    ASSERT_EQ(shape.pieces[1].material, DC_GPU_BODY_STONE);
    ASSERT_TRUE(dc_gpu_set_page(copy, 0, 0, UINT32_MAX, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(copy, 1, 0, UINT32_MAX, err, sizeof(err)));
    dc_gpu_body_t cold;
    ASSERT_TRUE(dc_gpu_rigid_step(copy, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(copy, body.id, &cold, err, sizeof(err)));
    ASSERT_EQ(cold.x_fp, body.x_fp);
    ASSERT_EQ(cold.y_fp, body.y_fp);
    ASSERT_TRUE(dc_gpu_set_page(copy, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(copy, 1, 0, 1, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)) &&
                dc_gpu_rigid_step(copy, err, sizeof(err)));
    dc_gpu_body_t continued, resumed;
    dc_gpu_body_motion_t continued_motion, resumed_motion;
    ASSERT_TRUE(
        dc_gpu_read_body_id(gpu, body.id, &continued, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_id(copy, body.id, &resumed, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, body.id, &continued_motion, err,
                                        sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(copy, body.id, &resumed_motion, err,
                                        sizeof(err)));
    printf("seam resume: y %d/%d vy %d/%d angle %.9f/%.9f cold vy %d original %d\n", continued.y_fp,resumed.y_fp,continued.vy_fp,resumed.vy_fp,continued_motion.angle,resumed_motion.angle,cold.vy_fp,body.vy_fp);
    ASSERT_EQ(continued.x_fp, resumed.x_fp);
    ASSERT_EQ(continued.y_fp, resumed.y_fp);
    ASSERT_EQ(continued_motion.angle, resumed_motion.angle);
    ASSERT_EQ(continued_motion.mass, resumed_motion.mass);
    dc_gpu_destroy(copy);
    dc_gpu_destroy(gpu);
    free(chunks);
    ASSERT_TRUE(extracted);
    ASSERT_EQ(material_cells, 0u);
    ASSERT_EQ(water, 16u * DC_FLUID_FULL);
    ASSERT_TRUE(fabsf(motion.mass - 26.4f) < .0001f);
    ASSERT_TRUE(fabsf(motion.density - 1.65f) < .00001f);
    ASSERT_TRUE(body.vy_fp > 0);
    ASSERT_TRUE(motion.angular_velocity > 0);
    PASS();
}
static void test_saturated_pool_retains_terrain_then_births_exactly_once(void) {
    dc_gpu_t *gpu = NULL;
    char err[256];
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    for (unsigned y = 20; y < 24; y++)
        for (unsigned x = 20; x < 24; x++)
            chunk->cells[y * 64 + x].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(gpu));
    for (unsigned i = 0; i < 64; i++) {
        dc_gpu_world_body_t body = {
            .body = {.width = 1, .height = 1, .id = 1000 + i, .active = 1},
            .chunk = {1000 + i, 1000}};
        ASSERT_TRUE(dc_gpu_spawn_world_body(gpu, body, err, sizeof(err)));
    }
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    bool retained = chunk->cells[20 * 64 + 20].material == DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_remove_body(gpu, 1000, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    bool born =
        dc_gpu_read_body_motion(gpu, 0x80000001u, &motion, err, sizeof(err));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t duplicate;
    bool twice =
        dc_gpu_read_body_id(gpu, 0x80000002u, &duplicate, err, sizeof(err));
    ASSERT_TRUE(dc_gpu_download_chunk(gpu, 0, chunk, err, sizeof(err)));
    bool consumed = chunk->cells[20 * 64 + 20].material == DC_MATERIAL_AIR;
    dc_gpu_destroy(gpu);
    free(chunk);
    ASSERT_TRUE(retained);
    ASSERT_TRUE(born);
    ASSERT_TRUE(!twice);
    ASSERT_TRUE(consumed);
    ASSERT_TRUE(fabsf(motion.mass - 43.2f) < .0001f);
    PASS();
}
int main(void) {
    RUN(test_wet_mixed_component_crosses_a_warm_chunk_seam);
    RUN(test_saturated_pool_retains_terrain_then_births_exactly_once);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
