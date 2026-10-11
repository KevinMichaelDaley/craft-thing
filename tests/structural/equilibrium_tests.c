#define main placement_fixture_main
#include "placement_tests.c"
#undef main

static bool near(double a, double b, double tolerance) {
    return fabs(a-b) <= tolerance * fmax(1, fabs(b));
}
static void test_self_weight_cantilever_equilibrium(void) {
    char e[256];
    dc_gpu_t *g=grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t root,tip;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&root));
    ASSERT_TRUE(fr_gpu_read_structure_node(g,2,&tip));
    const fr_structural_profile_t *p=fr_structural_profile(FR_STRUCTURAL_WOOD);
    double l=12,a=2,inertia=8.0/12,weight=p->density*a*900;
    double force=weight*l,moment=force*l/2;
    double expected=weight*pow(l,4)/(8*p->young*inertia)+
        weight*l*l/(2*(5.0/6)*p->shear*a)+force/1e8+l*moment/1e8;
    ASSERT_EQ(root.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_EQ(tip.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(near(root.reaction[0],0,.02));
    ASSERT_TRUE(near(root.reaction[1],-force,.002));
    ASSERT_TRUE(near(root.reaction[2],-moment,.002));
    ASSERT_TRUE(near(tip.displacement[1],expected,.002));
    dc_gpu_destroy(g); PASS();
}
static void test_released_root_is_a_mechanism(void) {
    char e[256]; dc_gpu_t *g=grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);
    s.anchors[0].kind=FR_JOINT_PINNED;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t root;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&root));
    ASSERT_EQ(root.state,FR_STRUCTURAL_UNSUPPORTED);
    dc_gpu_destroy(g); PASS();
}
static void test_two_pins_split_bridge_weight(void) {
    char e[256]; dc_gpu_t *g=grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);
    s.anchor_count=2;
    s.anchors[0].kind=FR_JOINT_PINNED;
    s.anchors[1]=s.anchors[0];
    s.anchors[1].id=2;s.anchors[1].node=2;s.anchors[1].x=32;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t a,b;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&a));
    ASSERT_TRUE(fr_gpu_read_structure_node(g,2,&b));
    ASSERT_EQ(a.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_EQ(b.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(near(a.reaction[1],-6480,.002));
    ASSERT_TRUE(near(b.reaction[1],-6480,.002));
    ASSERT_TRUE(near(a.reaction[2],0,.002));
    ASSERT_TRUE(near(b.reaction[2],0,.002));
    dc_gpu_destroy(g);PASS();
}
static void rigid_load_case(int center, double expected_a, double expected_b) {
    char e[256]; dc_gpu_t *g=grid(); ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);s.anchor_count=2;
    s.anchors[0].kind=FR_JOINT_PINNED;
    s.anchors[1]=s.anchors[0];s.anchors[1].id=2;
    s.anchors[1].node=2;s.anchors[1].x=32;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    dc_gpu_body_t load={.id=17,.x_fp=(center-1)<<16,.y_fp=37<<16,
        .width=2,.height=2,.active=1};
    ASSERT_TRUE(dc_gpu_spawn_body(g,load,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,17,0,0,DC_GPU_BODY_LOCK_ROTATION,e,sizeof(e)));
    for(unsigned t=0;t<40;t++) ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t a,b;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&a));
    ASSERT_TRUE(fr_gpu_read_structure_node(g,2,&b));
    printf("bridge reactions %.3f %.3f\n",a.reaction[1],b.reaction[1]);
    ASSERT_EQ(a.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_EQ(b.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(near(a.reaction[1],expected_a,.025));
    ASSERT_TRUE(near(b.reaction[1],expected_b,.025));
    ASSERT_TRUE(near(a.reaction[1]+b.reaction[1],-22680,.025));
    ASSERT_TRUE(near(b.reaction[1]*12,-12960*6-9720*(center-20),.025));
    dc_gpu_destroy(g); PASS();
}
static void test_centered_rigid_reactions(void) {rigid_load_case(26,-11340,-11340);}
static void test_eccentric_rigid_reactions(void) {rigid_load_case(23,-13770,-8910);}
int main(void) {
    RUN(test_self_weight_cantilever_equilibrium);
    RUN(test_released_root_is_a_mechanism);
    RUN(test_two_pins_split_bridge_weight);
    RUN(test_centered_rigid_reactions);
    RUN(test_eccentric_rigid_reactions);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
