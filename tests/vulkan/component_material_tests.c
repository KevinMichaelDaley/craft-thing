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

static dc_gpu_t *grid(bool wet) {
    char err[256];
    dc_gpu_t *gpu = NULL;
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    if (!chunk)
        return NULL;
    for (uint32_t y = 0; y < 64; y++)
        for (uint32_t x = 0; x < 64; x++) {
            dc_cell_t *cell = &chunk->cells[y * 64 + x];
            if (x == 0 || x == 63 || y == 63)
                cell->material = DC_MATERIAL_STONE;
            else if (wet && y >= 20)
                cell->fluid_mass = DC_FLUID_FULL;
        }
    bool okay = dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv",
                              err, sizeof(err)) &&
                dc_gpu_upload_chunk(gpu, 0, chunk, err, sizeof(err)) &&
                dc_gpu_set_page(gpu, 0, 0, 0, err, sizeof(err)) &&
                dc_gpu_set_rigid_solver(gpu, true);
    free(chunk);
    if (!okay) {
        dc_gpu_destroy(gpu);
        return NULL;
    }
    return gpu;
}

static bool pair(dc_gpu_t *gpu, char *err, uint32_t cap) {
    dc_gpu_world_body_t body = {.body = {.x_fp = 20 << 16,
                                         .y_fp = 30 << 16,
                                         .width = 4,
                                         .height = 4,
                                         .id = 100,
                                         .active = 1}};
    dc_gpu_compound_shape_t shape = {
        .count = 2,
        .pieces = {{.count = 4,
                    .material = DC_GPU_BODY_WOOD,
                    .vertices = {{0, 0},
                                 {2 << 16, 0},
                                 {2 << 16, 4 << 16},
                                 {0, 4 << 16}}},
                   {.count = 4,
                    .material = DC_GPU_BODY_STONE,
                    .vertices = {{2 << 16, 0},
                                 {4 << 16, 0},
                                 {4 << 16, 4 << 16},
                                 {2 << 16, 4 << 16}}}}};
    return dc_gpu_spawn_compound_body(gpu, body, &shape, err, cap) &&
           dc_gpu_set_body_origin(gpu, (dc_chunk_coord_t){0, 0}, err, cap);
}

static void
test_component_mass_center_inertia_and_density_follow_piece_materials(void) {
    char err[256];
    dc_gpu_t *gpu = grid(false);
    ASSERT_TRUE(gpu);
    bool okay = pair(gpu, err, sizeof(err));
    if (!okay)
        dc_gpu_destroy(gpu);
    ASSERT_TRUE(okay);
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 100, &motion, err, sizeof(err)));
    float center = (4.8f + 21.6f * 3.f) / 26.4f;
    float inertia = 4.8f * (20.f / 12.f + (1.f - center) * (1.f - center)) +
                    21.6f * (20.f / 12.f + (3.f - center) * (3.f - center));
    ASSERT_TRUE(fabsf(motion.mass - 26.4f) < .0001f);
    ASSERT_TRUE(fabsf(motion.density - 1.65f) < .0001f);
    ASSERT_TRUE(fabsf(motion.center_x - center) < .0001f);
    ASSERT_TRUE(fabsf(motion.center_y - 2.f) < .0001f);
    ASSERT_TRUE(fabsf(motion.inertia - inertia) < .001f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void test_mixed_component_sinks_and_turns_heavy_material_downward(void) {
    char err[256];
    dc_gpu_t *gpu = grid(true);
    ASSERT_TRUE(gpu);
    bool okay = pair(gpu, err, sizeof(err));
    if (!okay)
        dc_gpu_destroy(gpu);
    ASSERT_TRUE(okay);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    dc_gpu_body_t body;
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 100, &body, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 100, &motion, err, sizeof(err)));
    ASSERT_TRUE(fabsf((float)body.vy_fp / 65536.f - (.25f - 4.f / 26.4f)) <
                .002f);
    ASSERT_TRUE(motion.angular_velocity > .01f);
    dc_gpu_destroy(gpu);
    PASS();
}

static void
test_graph_checkpoint_retains_component_composition_and_next_tick(void) {
    char err[256];
    dc_gpu_t *gpu = grid(true);
    ASSERT_TRUE(gpu);
    bool okay = pair(gpu, err, sizeof(err));
    if (!okay)
        dc_gpu_destroy(gpu);
    ASSERT_TRUE(okay);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_impact_fracture(gpu));
    ASSERT_TRUE(fr_gpu_save_structure(
        gpu, "build/component_material_snapshot.bin", err, sizeof(err)));
    dc_gpu_t *copy = grid(true);
    ASSERT_TRUE(copy);
    ASSERT_TRUE(fr_gpu_load_structure(
        copy, "build/component_material_snapshot.bin", err, sizeof(err)));
    dc_gpu_compound_shape_t shape;
    ASSERT_TRUE(
        dc_gpu_read_compound_shape(copy, 100, &shape, err, sizeof(err)));
    ASSERT_EQ(shape.pieces[0].material, DC_GPU_BODY_WOOD);
    ASSERT_EQ(shape.pieces[1].material, DC_GPU_BODY_STONE);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu, err, sizeof(err)) &&
                dc_gpu_rigid_step(copy, err, sizeof(err)));
    dc_gpu_body_t a, b;
    dc_gpu_body_motion_t ma, mb;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu, 100, &a, err, sizeof(err)) &&
                dc_gpu_read_body_id(copy, 100, &b, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu, 100, &ma, err, sizeof(err)) &&
                dc_gpu_read_body_motion(copy, 100, &mb, err, sizeof(err)));
    ASSERT_EQ(a.x_fp, b.x_fp);
    ASSERT_EQ(a.y_fp, b.y_fp);
    ASSERT_EQ(a.vx_fp, b.vx_fp);
    ASSERT_EQ(a.vy_fp, b.vy_fp);
    ASSERT_EQ(ma.mass, mb.mass);
    ASSERT_EQ(ma.density, mb.density);
    ASSERT_EQ(ma.angle, mb.angle);
    ASSERT_EQ(ma.angular_velocity, mb.angular_velocity);
    dc_gpu_destroy(copy);
    dc_gpu_destroy(gpu);
    PASS();
}

int main(void) {
    RUN(test_component_mass_center_inertia_and_density_follow_piece_materials);
    RUN(test_mixed_component_sinks_and_turns_heavy_material_downward);
    RUN(test_graph_checkpoint_retains_component_composition_and_next_tick);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
