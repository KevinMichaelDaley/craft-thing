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

int main(void) {
    RUN(test_explicit_mortar_has_its_own_density_and_material_identity);
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
