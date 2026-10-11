#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static void test_explicit_mortar_has_its_own_density_and_material_identity(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    dc_gpu_body_motion_t motion;
    dc_gpu_body_shape_t shape;
    uint32_t id = FR_STRUCTURAL_BODY_ID_BASE + 1;
    ASSERT_TRUE(dc_gpu_read_body_motion(g, id, &motion, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_shape(g, id, &shape, err, sizeof(err)));
    ASSERT_INT_EQ(shape.material, DC_GPU_BODY_MORTAR);
    ASSERT_TRUE(fabs(motion.mass - 38.4) < .001);
    fr_structural_material_t material;
    ASSERT_TRUE(fr_structural_material_map(false, shape.material, &material));
    ASSERT_INT_EQ(material, FR_STRUCTURAL_MORTAR);
    ASSERT_TRUE(!fr_structural_material_map(true, DC_MATERIAL_DIRT, &material));
    ASSERT_TRUE(!fr_structural_material_map(true, DC_MATERIAL_SAND, &material));
    dc_gpu_destroy(g);
    PASS();
}

static void test_overloaded_mortar_crumble_preserves_mass_without_launch(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_download_chunk(g, 0, chunk, err, sizeof(err)));
    uint64_t mass = 0;
    int64_t momentum_x = 0, momentum_y = 0;
    for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
        const dc_mpm_particle_t *p = &chunk->particles[i];
        if (!p->mass_fp) continue;
        ASSERT_INT_EQ(p->material, DC_MATERIAL_MORTAR_DEBRIS);
        mass += p->mass_fp;
        momentum_x += (int64_t)p->mass_fp * p->vx_fp;
        momentum_y += (int64_t)p->mass_fp * p->vy_fp;
    }
    ASSERT_TRUE(fabs((double)mass / DC_FLUID_FULL - 38.4) < .002);
    ASSERT_INT_EQ(chunk->particle_count, 24);
    ASSERT_EQ(momentum_x, 0);
    ASSERT_EQ(momentum_y, 0);
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(g, FR_STRUCTURAL_BODY_ID_BASE + 1,
                                   &body, err, sizeof(err)));
    ASSERT_TRUE(!body.active);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

static void test_crumble_accounts_fracture_work_and_remaining_dissipation(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    ASSERT_TRUE(dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    fr_structural_gpu_member_state_t state;
    ASSERT_TRUE(fr_gpu_read_structure_member(g, 1, &state));
    ASSERT_EQ(state.state, FR_STRUCTURAL_CONVERTED);
    ASSERT_TRUE(state.fracture_work > 0);
    ASSERT_TRUE(state.fracture_work <= state.strain_energy);
    ASSERT_TRUE(state.dissipated_energy > 0);
    ASSERT_TRUE(fabs(state.fracture_work + state.dissipated_energy -
                     state.strain_energy) < .01);
    dc_gpu_destroy(g);
    PASS();
}

static void test_rotated_crumble_across_chunk_seam_preserves_centroid(void) {
    char err[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(58);
    s.members[0].material = FR_STRUCTURAL_MORTAR;
    const double angle = .3;
    s.nodes[1].chunk.x = 1;
    s.nodes[1].x_fp = (int32_t)llround((58 + 12 * cos(angle) - 64) * 65536);
    s.nodes[1].y_fp = (int32_t)llround((40 + 12 * sin(angle)) * 65536);
    s.anchors[0].x = 57;
    s.anchors[0].y = 39;
    ASSERT_TRUE(dc_gpu_paint_material(g, 57, 39, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, err, sizeof(err)));
    for (unsigned i = 0; i < 3; i++)
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    ASSERT_TRUE(chunk);
    double mass = 0, x = 0, y = 0;
    unsigned count = 0;
    for (unsigned slot = 0; slot < 2; slot++) {
        ASSERT_TRUE(dc_gpu_download_chunk(g, slot, chunk, err, sizeof(err)));
        unsigned actual = 0;
        for (unsigned i = 0; i < DC_MPM_PARTICLES_PER_CHUNK; i++) {
            const dc_mpm_particle_t *p = &chunk->particles[i];
            if (!p->mass_fp) continue;
            ASSERT_INT_EQ(p->material, DC_MATERIAL_MORTAR_DEBRIS);
            double m = (double)p->mass_fp / 65536;
            mass += m;
            x += m * ((double)p->x_fp / 65536 + slot * 64);
            y += m * (double)p->y_fp / 65536;
            actual++;
        }
        ASSERT_EQ(actual, chunk->particle_count);
        count += actual;
    }
    ASSERT_TRUE(count > 24);
    ASSERT_TRUE(fabs(mass - 38.4) < .002);
    ASSERT_TRUE(fabs(x / mass - (58 + 6 * cos(angle))) < .002);
    ASSERT_TRUE(fabs(y / mass - (40 + 6 * sin(angle))) < .002);
    free(chunk);
    dc_gpu_destroy(g);
    PASS();
}

int main(void) {
    RUN(test_explicit_mortar_has_its_own_density_and_material_identity);
    RUN(test_overloaded_mortar_crumble_preserves_mass_without_launch);
    RUN(test_crumble_accounts_fracture_work_and_remaining_dissipation);
    RUN(test_rotated_crumble_across_chunk_seam_preserves_centroid);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
