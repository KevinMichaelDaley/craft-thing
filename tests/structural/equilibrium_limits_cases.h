#ifndef FERRUM_TESTS_STRUCTURAL_EQUILIBRIUM_LIMITS_H
#define FERRUM_TESTS_STRUCTURAL_EQUILIBRIUM_LIMITS_H
static void test_long_bonded_chain_is_not_a_hinge_mechanism(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.node_count = 32;
    s.member_count = 16;
    s.joint_count = 15;
#ifdef DC_QUARTER_NATIVE_VIEW
    const unsigned segment = 16;
#else
    const unsigned segment = 6;
#endif
    for (unsigned m = 0; m < 16; m++) {
        for (unsigned end = 0; end < 2; end++) {
            unsigned x = 20 + segment * (m + end), n = 2 * m + end;
            s.nodes[n] =
                (fr_structural_gpu_node_t){.id = n + 1,
                                           .generation = 1,
                                           .chunk = {x / 64, 0},
                                           .x_fp = (int32_t)(x % 64) << 16,
                                           .y_fp = 40 << 16};
        }
        s.members[m] = (fr_structural_gpu_member_t){
            m + 1, 1, 2 * m + 1, 2 * m + 2, FR_STRUCTURAL_WOOD, 2 << 16, 0, 0};
        if (m < 15)
            s.joints[m] =
                (fr_structural_gpu_joint_t){.id = m + 1,
                                            .generation = 1,
                                            .a = 2 * m + 2,
                                            .b = 2 * m + 3,
                                            .kind = FR_JOINT_MOMENT,
                                            .normal_stiffness = 1e8,
                                            .shear_stiffness = 1e8,
                                            .rotational_stiffness = 1e8,
                                            .bearing_capacity = 1e7,
                                            .pullout_capacity = 1e7,
                                            .shear_capacity = 1e7,
                                            .moment_capacity = 1e7};
    }
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    fr_structural_gpu_node_state_t n;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &n));
    printf("long bonded chain state %u\n", n.state);
    ASSERT_TRUE(n.state != FR_STRUCTURAL_UNSUPPORTED);
    dc_gpu_destroy(g);
    PASS();
}
static void test_numerical_failure_does_not_poison_another_component(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    s.node_count = 4;
    s.member_count = 2;
    s.anchor_count = 2;
    s.nodes[2] = s.nodes[0];
    s.nodes[2].id = 3;
    s.nodes[2].y_fp = 60 << 16;
    s.nodes[3] = s.nodes[1];
    s.nodes[3].id = 4;
    s.nodes[3].y_fp = 60 << 16;
    s.members[1] = s.members[0];
    s.members[1].id = 2;
    s.members[1].a = 3;
    s.members[1].b = 4;
    s.anchors[1] = s.anchors[0];
    s.anchors[1].id = 2;
    s.anchors[1].node = 3;
    s.anchors[1].y = 61;
    s.anchors[0].normal_stiffness = s.anchors[0].shear_stiffness =
        s.anchors[0].rotational_stiffness = 1e-20;
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 61, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    fr_structural_gpu_node_state_t weak, good;
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 1, &weak));
    ASSERT_TRUE(fr_gpu_read_structure_node(g, 3, &good));
    ASSERT_EQ(weak.state, FR_STRUCTURAL_UNKNOWN);
    ASSERT_EQ(good.state, FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(near(good.reaction[1], -12960, .002));
    dc_gpu_destroy(g);
    PASS();
}
static void test_contact_overflow_is_unknown_until_contacts_recover(void) {
    char e[256];
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s = beam(20);
    ASSERT_TRUE(
        dc_gpu_paint_material(g, 20, 41, 0, DC_MATERIAL_STONE, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g, &s, e, sizeof(e)));
    dc_gpu_body_t load = {.id = 17,
                          .x_fp = 25 << 16,
                          .y_fp = 37 << 16,
                          .width = 2,
                          .height = 2,
                          .active = 1};
    ASSERT_TRUE(dc_gpu_spawn_body(g, load, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_set_contact_capacity(g, 0, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    fr_structural_gpu_component_state_t component;
    ASSERT_TRUE(fr_gpu_read_structure_component(g, 1, &component));
    ASSERT_EQ(component.state, FR_STRUCTURAL_UNKNOWN);
    ASSERT_TRUE(component.flags & FR_STRUCTURAL_DIAG_LOAD_OVERFLOW);
    ASSERT_TRUE(
        dc_gpu_set_contact_capacity(g, DC_GPU_CONTACT_CAPACITY, e, sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g, e, sizeof(e)));
    ASSERT_TRUE(fr_gpu_read_structure_component(g, 1, &component));
    ASSERT_EQ(component.state, FR_STRUCTURAL_SUPPORTED);
    dc_gpu_destroy(g);
    PASS();
}
#endif
