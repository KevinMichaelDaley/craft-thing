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
        ASSERT_INT_EQ(p->material, 6);
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

int main(void) {
    RUN(test_explicit_mortar_has_its_own_density_and_material_identity);
    RUN(test_overloaded_mortar_crumble_preserves_mass_without_launch);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
