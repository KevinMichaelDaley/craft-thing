#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static bool column_utilization(unsigned segments, float *utilization,
                               float *critical) {
    char err[256];
    dc_gpu_t *g = grid();
    if (!g)
        return false;
    fr_structural_gpu_graph_t s = beam(50);
    s.node_count = segments + 1;
    s.member_count = segments;
    for (unsigned i = 0; i <= segments; i++) {
        s.nodes[i] = s.nodes[0];
        s.nodes[i].id = i + 1;
        s.nodes[i].x_fp = 50 << 16;
        s.nodes[i].y_fp = (60 - i * 12 / segments) << 16;
    }
    for (unsigned i = 0; i < segments; i++)
        s.members[i] = (fr_structural_gpu_member_t){
            i + 1, 1, i + 1, i + 2, FR_STRUCTURAL_WOOD, 2 << 16, 0, 0};
    s.anchors[0].y = 61;
    bool okay = dc_gpu_paint_material(g, 50, 61, 0, DC_MATERIAL_STONE, err,
                                      sizeof(err)) &&
                fr_gpu_place_structure(g, &s, err, sizeof(err));
    dc_gpu_body_t load = {.x_fp = 42 << 16,
                          .y_fp = 32 << 16,
                          .width = 16,
                          .height = 16,
                          .id = 7,
                          .active = 1};
    okay = okay && dc_gpu_spawn_body(g, load, err, sizeof(err));
    for (unsigned i = 0; okay && i < 8; i++)
        okay = dc_gpu_rigid_step(g, err, sizeof(err));
    fr_structural_gpu_member_state_t state;
    okay = okay && fr_gpu_read_structure_member(g, 1, &state) &&
           (state.state == FR_STRUCTURAL_SUPPORTED ||
            state.state == FR_STRUCTURAL_OVERLOADED);
    if (okay) {
        *utilization = state.utilization;
        *critical = -state.axial / state.utilization;
        printf("column segments%u: axial%f utilization%f\n", segments,
               state.axial, state.utilization);
    } else
        printf("column setup/solve: %s\n", err);
    dc_gpu_destroy(g);
    return okay;
}

static void test_collinear_subdivision_does_not_strengthen_a_column(void) {
    float whole, split, whole_critical, split_critical;
    ASSERT_TRUE(column_utilization(1, &whole, &whole_critical));
    ASSERT_TRUE(column_utilization(2, &split, &split_critical));
    ASSERT_TRUE(whole > 1);
    ASSERT_TRUE(split > 1);
    ASSERT_TRUE(fabs(whole_critical - split_critical) < whole_critical * .0001);
    PASS();
}

int main(void) {
    RUN(test_collinear_subdivision_does_not_strengthen_a_column);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
