#define main fixture_main
#include "placement_tests.c"
#undef main

static double g_rigid_mean;
static void test_static_structural_graph_stays_within_gpu_budget(void) {
    ASSERT_TRUE(g_rigid_mean < 14.0);
    PASS();
}
int main(int argc, char **argv) {
    bool automatic = argc > 2 && strcmp(argv[2], "--automatic") == 0;
    char err[256];
    dc_gpu_t *g = grid();
    if (!g)
        return 1;
    fr_structural_gpu_graph_t s = {
        .node_count = 32, .member_count = 16, .anchor_count = 16};
    for (unsigned i = 0; i < 16; i++) {
        int x = 20 + (i % 4) * 16, y = 30 + (i / 4) * 16;
        fr_structural_gpu_graph_t one = beam(x);
        for (unsigned j = 0; j < 2; j++) {
            one.nodes[j].id = 2 * i + j + 1;
            one.nodes[j].chunk.y = y / 64;
            one.nodes[j].y_fp = (y % 64) << 16;
            s.nodes[2 * i + j] = one.nodes[j];
        }
        one.members[0].id = i + 1;
        one.members[0].a = 2 * i + 1;
        one.members[0].b = 2 * i + 2;
        s.members[i] = one.members[0];
        one.anchors[0].id = i + 1;
        one.anchors[0].node = 2 * i + 1;
        one.anchors[0].chunk.y = (y + 1) / 64;
        one.anchors[0].y = (y + 1) % 64;
        s.anchors[i] = one.anchors[0];
        for (int support_x = automatic ? 0 : x; support_x <= x; support_x++)
            if (!dc_gpu_paint_material(g, support_x, y + 1, 0,
                                       DC_MATERIAL_STONE, err, sizeof(err)))
                return 2;
    }
    if (!fr_gpu_place_structure(g, &s, err, sizeof(err))) {
        puts(err);
        return 2;
    }
    if (automatic && !dc_gpu_enable_automatic_components(g))
        return 2;
    for (unsigned i = 0; i < 8; i++)
        if (!dc_gpu_rigid_step(g, err, sizeof(err)))
            return 3;
    uint64_t rigid = 0, total = 0;
    float residual = 0, pivot = 1;
    unsigned active = 0, refinement = 0;
    for (unsigned i = 0; i < 8; i++) {
        dc_gpu_tick_capture_t c;
        if (!dc_gpu_tick_capture(g, &c, err, sizeof(err)))
            return 4;
        rigid += c.stages[0].gpu_ns;
        for (unsigned j = 0; j < 3; j++)
            total += c.stages[j].gpu_ns;
        for (unsigned j = 0; j < 16; j++) {
            fr_structural_gpu_component_state_t component;
            if (!fr_gpu_read_structure_component(g, 2 * j + 1, &component) ||
                component.state != FR_STRUCTURAL_SUPPORTED)
                return 5;
            residual = fmaxf(residual, component.relative_residual);
            pivot = fminf(pivot, component.minimum_pivot);
            active = component.active_iterations > active
                         ? component.active_iterations
                         : active;
            refinement = component.refinement_iterations > refinement
                             ? component.refinement_iterations
                             : refinement;
        }
    }
    dc_gpu_memory_stats_t memory;
    if (!dc_gpu_memory_stats(g, &memory))
        return 6;
    printf("Quarter-native 16-component mean GPU: rigid/structure %.3f ms; "
           "whole tick %.3f ms\n",
           rigid / 8000000.0, total / 8000000.0);
    printf("mapped %.3f MiB, device %.3f MiB; max residual %.9g, minimum pivot "
           "%.9g, max active %u/refinement %u\n",
           memory.mapped_local_bytes / 1048576.0,
           memory.device_only_bytes / 1048576.0, residual, pivot, active,
           refinement);
    dc_gpu_destroy(g);
    if (argc > 1 && strcmp(argv[1], "--check-budget") == 0) {
        g_rigid_mean = rigid / 8000000.0;
        RUN(test_static_structural_graph_stays_within_gpu_budget);
        printf("%d passed, %d failed\n", g_pass, g_fail);
    }
    return g_fail != 0;
}
