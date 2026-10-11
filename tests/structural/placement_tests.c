#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
            printf("FAIL %d: %s\n", __LINE__, #e);                             \
            g_fail++;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++
static dc_gpu_t *grid(void) {
    dc_gpu_t *g = NULL;
    char err[256];
    dc_chunk_t *c = calloc(1, sizeof(*c));
    bool ok = c && dc_gpu_create(&g, 128, 128, "build/shaders/pattern.comp.spv",
                                 err, sizeof(err));
    for (uint32_t i = 0; ok && i < 4; i++) {
        c->coord = (dc_chunk_coord_t){i % 2, i / 2};
        ok = dc_gpu_upload_chunk(g, i, c, err, sizeof(err)) &&
             dc_gpu_set_page(g, i % 2, i / 2, i, err, sizeof(err));
    }
    free(c);
    if (!ok) {
        dc_gpu_destroy(g);
        return NULL;
    }
    dc_gpu_set_rigid_solver(g, true);
    return g;
}
static fr_structural_gpu_graph_t beam(int x) {
    fr_structural_gpu_graph_t s = {
        .node_count = 2, .member_count = 1, .anchor_count = 1};
    s.nodes[0] = (fr_structural_gpu_node_t){.id = 1,
                                            .generation = 1,
                                            .chunk = {x / 64, 0},
                                            .x_fp = (x % 64) << 16,
                                            .y_fp = 40 << 16};
    s.nodes[1] = s.nodes[0];
    s.nodes[1].id = 2;
    s.nodes[1].x_fp += 12 << 16;
    if (s.nodes[1].x_fp >= 64 << 16) {
        s.nodes[1].x_fp -= 64 << 16;
        s.nodes[1].chunk.x++;
    }
    s.members[0] = (fr_structural_gpu_member_t){
        1, 1, 1, 2, FR_STRUCTURAL_WOOD, 2 << 16, 0, 0};
    s.anchors[0] =
        (fr_structural_gpu_anchor_t){.id = 1,
                                     .generation = 1,
                                     .node = 1,
                                     .kind = FR_JOINT_MOMENT,
                                     .chunk = {x / 64, 0},
                                     .x = x % 64,
                                     .y = 41,
                                     .width = 1,
                                     .height = 1,
                                     .terrain_material = DC_MATERIAL_STONE,
                                     .normal_stiffness = 1e8,
                                     .shear_stiffness = 1e8,
                                     .rotational_stiffness = 1e8,
                                     .bearing_capacity = 1e7,
                                     .pullout_capacity = 1e7,
                                     .shear_capacity = 1e7,
                                     .moment_capacity = 1e7,
                                     .friction = .5};
    return s;
}
static void test_explicit_proxy_and_atomic_validation(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &n));
    ASSERT_EQ(n.valid_anchors, 1u);
    dc_gpu_body_motion_t m;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &m,
                                        e, sizeof(e)));
    ASSERT_TRUE(fabsf(m.mass - 14.4f) < .001f);
    dc_gpu_world_body_t before, after;
    ASSERT_TRUE(dc_gpu_read_world_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                       &before, e, sizeof(e)));
    for (unsigned i = 0; i < 20; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_world_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                       &after, e, sizeof(e)));
    ASSERT_EQ(memcmp(&before, &after, sizeof(before)), 0);
    s.members[0].depth_fp = 0;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_read_world_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                       &after, e, sizeof(e)));
    ASSERT_EQ(memcmp(&before, &after, sizeof(before)), 0);
    dc_gpu_destroy(g);
    PASS();
}
static void test_cold_anchor_and_slot_identity(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(60);
    s.anchors[0].x = 63;
    s.anchors[0].width = 2;
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 63, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 64, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_set_page(g, 1, 0, DC_GPU_CHUNK_SLOTS, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &n));
    ASSERT_EQ(n.state, FR_STRUCTURAL_UNKNOWN);
    dc_chunk_t *c = calloc(1, sizeof(*c));
    ASSERT_TRUE(c);
    c->coord = (dc_chunk_coord_t){99, 0};
    c->cells[41 * 64].material = DC_MATERIAL_STONE;
    ASSERT_TRUE(dc_gpu_upload_chunk(g, 1, c, e, sizeof(e)));
    free(c);
    ASSERT_TRUE(dc_gpu_set_page(g, 1, 0, 1, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &n));
    ASSERT_EQ(n.state, FR_STRUCTURAL_UNKNOWN);
    dc_gpu_destroy(g);
    PASS();
}
static void test_marking_ownership_and_rejection(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_STONE;
    s.members[0].flags = FR_STRUCTURAL_MARK_TERRAIN;
    s.anchor_count = 0;
    for (uint32_t y = 39; y < 41; y++)
        for (uint32_t x = 20; x < 32; x++)
            ASSERT_TRUE(dc_gpu_paint_material(g, x, y, 0, DC_MATERIAL_STONE, e,
                                              sizeof(e)));
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 25, 40, 0, DC_MATERIAL_SAND, e, sizeof(e)));
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_chunk_t *c = calloc(1, sizeof(*c));
    ASSERT_TRUE(c);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, c, e, sizeof(e)));
    ASSERT_EQ(c->cells[39 * 64 + 20].material, DC_MATERIAL_STONE);
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 25, 40, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, c, e, sizeof(e)));
    for (uint32_t y = 39; y < 41; y++)
        for (uint32_t x = 20; x < 32; x++)
            ASSERT_EQ(c->cells[y * 64 + x].material, DC_MATERIAL_AIR);
    free(c);
    uint32_t o;
    ASSERT_TRUE(dc_gpu_read_occupancy(g, 25, 40, &o, NULL, e, sizeof(e)));
    ASSERT_EQ(o, FR_STRUCTURAL_BODY_ID_BASE + 1);
    dc_gpu_destroy(g);
    PASS();
}
int main(void) {
    RUN(test_explicit_proxy_and_atomic_validation);
    RUN(test_cold_anchor_and_slot_identity);
    RUN(test_marking_ownership_and_rejection);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
