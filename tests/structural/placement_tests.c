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
#ifdef DC_QUARTER_NATIVE_VIEW
    const uint32_t width = 640, height = 448;
#else
    const uint32_t width = 128, height = 128;
#endif
#ifdef DC_STRUCTURAL_PRESENTATION_TEST
    bool ok = c && dc_gpu_create_window(&g, width, height, 640, 448,
                                        "build/shaders/pattern.comp.spv", err, sizeof(err));
    if (ok)
        ok = dc_gpu_set_viewport(g, 0, 0, 160, 112) && dc_gpu_set_display_zoom(g, 4);
#else
    bool ok = c && dc_gpu_create(&g, width, height, "build/shaders/pattern.comp.spv",
                                err, sizeof(err));
#endif
    uint32_t pages_x = width / 64, pages_y = height / 64;
    for (uint32_t i = 0; ok && i < pages_x * pages_y; i++) {
        c->coord = (dc_chunk_coord_t){i % pages_x, i / pages_x};
        ok = dc_gpu_upload_chunk(g, i, c, err, sizeof(err)) &&
             dc_gpu_set_page(g, i % pages_x, i / pages_x, i, err, sizeof(err));
    }
    free(c);
    if (!ok) {
        dc_gpu_destroy(g);
        return NULL;
    }
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
    ASSERT_TRUE(!dc_gpu_set_rigid_solver(g, false));
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
    fr_structural_gpu_graph_t s = beam(63);
    s.anchors[0].x = 63;
    s.anchors[0].width = 2;
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 63, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 64, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_set_page(g, 1, 0, UINT32_MAX, e, sizeof(e)));
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
static void test_structural_ownership_cannot_be_overwritten(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + 1;
    ASSERT_TRUE(!dc_gpu_set_body_motion(g, id, 0, 0, 0, e, sizeof(e)));
    ASSERT_TRUE(!dc_gpu_remove_body(g, id, e, sizeof(e)));
    dc_gpu_world_body_t w;
    ASSERT_TRUE(dc_gpu_read_world_body(g, id, &w, e, sizeof(e)));
    ASSERT_TRUE(!dc_gpu_spawn_world_body(g, w, e, sizeof(e)));
    dc_gpu_destroy(g);
    PASS();
}
static void test_anchor_must_attach_to_the_member(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchors[0].chunk.x = 1000;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(!fr_gpu_read_structure_node(g, 1, &n));
    dc_gpu_destroy(g);
    PASS();
}
static void test_capacity_growth_keeps_sources(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    for (uint32_t i = 0; i < DC_GPU_BODY_CAPACITY; i++) {
        dc_gpu_body_t b = {.x_fp = (int32_t)((i % 32) * 3) << 16,
                           .y_fp = (int32_t)(80 + i / 32 * 3) << 16,
                           .width = 1,
                           .height = 1,
                           .id = i + 1,
                           .active = 1};
        ASSERT_TRUE(dc_gpu_spawn_body(g, b, e, sizeof(e)));
    }
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_gpu_body_t b;
    ASSERT_TRUE(dc_gpu_read_body_id(g, 64, &b, e, sizeof(e)));
    ASSERT_EQ(b.active, 1u);
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &n));
    dc_gpu_destroy(g);
    PASS();
}
static void test_explicit_joint_graph_does_not_multiply_roots(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.node_count = 4;
    s.member_count = 2;
    s.joint_count = 2;
    s.nodes[1].x_fp = 26 << 16;
    s.nodes[2] = s.nodes[1];
    s.nodes[2].id = 3;
    s.nodes[3] = s.nodes[1];
    s.nodes[3].id = 4;
    s.nodes[3].x_fp = 32 << 16;
    s.members[1] = s.members[0];
    s.members[1].id = 2;
    s.members[1].a = 3;
    s.members[1].b = 4;
    s.joints[0] = (fr_structural_gpu_joint_t){.id = 1,
                                              .generation = 1,
                                              .a = 2,
                                              .b = 3,
                                              .kind = FR_JOINT_BONDED,
                                              .normal_stiffness = 1e7,
                                              .shear_stiffness = 1e7,
                                              .rotational_stiffness = 1e7,
                                              .bearing_capacity = 1e6,
                                              .pullout_capacity = 1e6,
                                              .shear_capacity = 1e6,
                                              .moment_capacity = 1e6};
    s.joints[1] = s.joints[0];
    s.joints[1].id = 2;
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 4, &n));
    ASSERT_EQ(n.component, 1u);
    ASSERT_EQ(n.valid_anchors, 1u);
    s.joints[0].normal_stiffness = INFINITY;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 4, &n));
    ASSERT_EQ(n.valid_anchors, 1u);
    s.joints[0].normal_stiffness=1e7;
    s.joint_count=0;
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_read_structure_node(g,4,&n));
    ASSERT_EQ(n.component,3u);
    ASSERT_EQ(n.valid_anchors,0u);
    ASSERT_EQ(n.state,FR_STRUCTURAL_UNSUPPORTED);
    dc_gpu_destroy(g);
    PASS();
}
static void test_body_identity_collision_is_atomic(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    dc_gpu_body_t original = {.x_fp = 70 << 16,
                              .y_fp = 80 << 16,
                              .width = 1,
                              .height = 1,
                              .id = FR_STRUCTURAL_BODY_ID_BASE + 1,
                              .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, original, e, sizeof(e)));
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_gpu_body_t after;
    ASSERT_TRUE(dc_gpu_read_body_id(g, original.id, &after, e, sizeof(e)));
    ASSERT_EQ(memcmp(&after, &original, sizeof(after)), 0);
    dc_gpu_destroy(g);
    PASS();
}
static void test_rest_geometry_requires_new_generation(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.anchor_count = 0;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    s.nodes[0].x_fp += 1 << 16;
    s.nodes[1].x_fp += 1 << 16;
    ASSERT_TRUE(!fr_gpu_place_structure(g, &s, e, sizeof(e)));
    s.nodes[0].generation++;
    s.nodes[1].generation++;
    s.members[0].generation++;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_gpu_world_body_t w;
    ASSERT_TRUE(dc_gpu_read_world_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1, &w, e,
                                       sizeof(e)));
    ASSERT_EQ(w.body.x_fp, 21 << 16);
    dc_gpu_destroy(g);
    PASS();
}
static void test_body_only_snapshot_cannot_drop_structural_identity(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);fr_structural_gpu_graph_t s=beam(20);
    s.anchor_count=0;ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(!dc_gpu_save_bodies(g,"build/structural_body_only.bin",e,sizeof(e)));
    fr_structural_gpu_node_state_t n;ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&n));
    dc_gpu_destroy(g);remove("build/structural_body_only.bin");PASS();
}
static void test_body_only_load_cannot_delete_structural_sources(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_save_bodies(g,"build/structural_empty_body_only.bin",e,sizeof(e)));
    fr_structural_gpu_graph_t s=beam(20);s.anchor_count=0;
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(!dc_gpu_load_bodies(g,"build/structural_empty_body_only.bin",e,sizeof(e)));
    dc_gpu_body_t b;ASSERT_TRUE(dc_gpu_read_body_id(g,FR_STRUCTURAL_BODY_ID_BASE+1,&b,e,sizeof(e)));
    ASSERT_EQ(b.active,1u);dc_gpu_destroy(g);remove("build/structural_empty_body_only.bin");PASS();
}
int main(void) {
    RUN(test_explicit_proxy_and_atomic_validation);
    RUN(test_cold_anchor_and_slot_identity);
    RUN(test_marking_ownership_and_rejection);
    RUN(test_structural_ownership_cannot_be_overwritten);
    RUN(test_capacity_growth_keeps_sources);
    RUN(test_anchor_must_attach_to_the_member);
    RUN(test_explicit_joint_graph_does_not_multiply_roots);
    RUN(test_body_identity_collision_is_atomic);
    RUN(test_rest_geometry_requires_new_generation);
    RUN(test_body_only_snapshot_cannot_drop_structural_identity);
    RUN(test_body_only_load_cannot_delete_structural_sources);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
