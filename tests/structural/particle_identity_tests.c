#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void test_live_mpm_descendants_retain_their_root_across_checkpoint(void) {
    const char *path = "build/structural_particle_checkpoint.bin";
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(20);
    graph.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g, path, err, sizeof(err)));
    dc_gpu_destroy(g); g = grid(); ASSERT_TRUE(g);
    ASSERT_TRUE(fr_gpu_load_structure(g, path, err, sizeof(err)));
    remove(path);
    ASSERT_TRUE(!dc_gpu_remove_body(g, FR_STRUCTURAL_BODY_ID_BASE + 1, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->particle_count > 0);
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        if (!chunk->particles[i].mass_fp) continue;
        ASSERT_TRUE(chunk->particles[i].id_lo & 0x80000000u);
        ASSERT_EQ(chunk->particles[i].id_hi, 1);
    }
    free(chunk); dc_gpu_destroy(g); PASS();
}

static void test_separate_crumbling_roots_have_distinct_particle_identities(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(20), other = beam(84);
    graph.node_count = 4; graph.member_count = 2; graph.anchor_count = 2;
    graph.members[0].material = FR_STRUCTURAL_MORTAR;
    for (unsigned i = 0; i < 2; i++) {
        graph.nodes[2 + i] = other.nodes[i];
        graph.nodes[2 + i].id += 2;
        graph.nodes[2 + i].generation = 2;
    }
    graph.members[1] = other.members[0];
    graph.members[1].id = 2; graph.members[1].generation = 2;
    graph.members[1].a = 3; graph.members[1].b = 4;
    graph.members[1].material = FR_STRUCTURAL_MORTAR;
    graph.anchors[1] = other.anchors[0];
    graph.anchors[1].id = 2; graph.anchors[1].generation = 2; graph.anchors[1].node = 3;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g, 84, 41, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *a = calloc(1, sizeof(*a)), *b = calloc(1, sizeof(*b)); ASSERT_TRUE(a && b);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, a, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g, 1, b, err, sizeof(err)));
    ASSERT_TRUE(a->particle_count > 0 && b->particle_count > 0);
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        if (!a->particles[i].mass_fp) continue;
        for (unsigned j = 0; j < DC_MPM_PARTICLES_PER_CHUNK; j++) {
            if (!b->particles[j].mass_fp) continue;
            ASSERT_TRUE(a->particles[i].id_lo != b->particles[j].id_lo ||
                        a->particles[i].id_hi != b->particles[j].id_hi);
        }
    }
    free(a); free(b); dc_gpu_destroy(g); PASS();
}

static void test_tiny_clipped_cells_do_not_create_zero_mass_particles(void) {
    char err[256]; dc_gpu_t *g = grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t graph = beam(20);
    graph.members[0].material = FR_STRUCTURAL_MORTAR;
    graph.nodes[1].y_fp--;
    graph.anchors[0].x = 19; graph.anchors[0].y = 38;
    ASSERT_TRUE(dc_gpu_paint_material(g, 19, 38, 0, DC_MATERIAL_STONE, err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &graph, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk)); ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    ASSERT_TRUE(chunk->particle_count > 0);
    unsigned alive = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) alive += chunk->particles[i].mass_fp != 0;
    ASSERT_EQ(alive, chunk->particle_count);
    free(chunk); dc_gpu_destroy(g); PASS();
}

int main(void) {
    RUN(test_live_mpm_descendants_retain_their_root_across_checkpoint);
    RUN(test_separate_crumbling_roots_have_distinct_particle_identities);
    RUN(test_tiny_clipped_cells_do_not_create_zero_mass_particles);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
