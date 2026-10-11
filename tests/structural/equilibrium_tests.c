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
    fr_structural_gpu_component_state_t budget;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&budget));
    ASSERT_TRUE(budget.relative_residual<1e-4);
    for(unsigned axis=0;axis<3;axis++)
        ASSERT_TRUE(near(budget.external[axis]+budget.reaction[axis],0,.1));
    dc_gpu_destroy(g); PASS();
}
static void test_centered_rigid_reactions(void) {rigid_load_case(26,-11340,-11340);}
static void test_eccentric_rigid_reactions(void) {rigid_load_case(23,-13770,-8910);}
static void test_material_stress_drives_overload(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);s.members[0].material=FR_STRUCTURAL_STONE;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_member_state_t member;
    ASSERT_TRUE(fr_gpu_read_structure_member(g,1,&member));
    ASSERT_EQ(member.state,FR_STRUCTURAL_OVERLOADED);
    ASSERT_TRUE(near(member.shear,58320,.003));
    ASSERT_TRUE(near(fabs(member.moment),349920,.003));
    ASSERT_TRUE(near(member.utilization,3.645,.003));
    ASSERT_TRUE(member.utilization_location<.01);
    ASSERT_TRUE(member.strain_energy>0);
    dc_gpu_destroy(g);PASS();
}
static void test_weak_numerical_system_is_unknown(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);
    s.anchors[0].normal_stiffness=1e-20;
    s.anchors[0].shear_stiffness=1e-20;
    s.anchors[0].rotational_stiffness=1e-20;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t node;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&node));
    ASSERT_EQ(node.state,FR_STRUCTURAL_UNKNOWN);
    dc_gpu_destroy(g);PASS();
}
static void test_oriented_bearing_opens_in_tension(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);s.anchor_count=2;
    s.anchors[0].kind=FR_JOINT_BEARING;s.anchors[0].normal_angle=1.57079632679f;
    s.anchors[1]=s.anchors[0];s.anchors[1].id=2;s.anchors[1].node=2;s.anchors[1].x=32;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t node;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&node));
    ASSERT_EQ(node.state,FR_STRUCTURAL_UNSUPPORTED);
    dc_gpu_destroy(g);PASS();
}
static void test_joint_capacity_is_finite(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);s.node_count=4;s.member_count=2;s.joint_count=1;
    s.nodes[1].x_fp=26<<16;s.nodes[2]=s.nodes[1];s.nodes[2].id=3;
    s.nodes[3]=s.nodes[1];s.nodes[3].id=4;s.nodes[3].x_fp=32<<16;
    s.members[1]=s.members[0];s.members[1].id=2;s.members[1].a=3;s.members[1].b=4;
    s.joints[0]=(fr_structural_gpu_joint_t){.id=1,.generation=1,.a=2,.b=3,.kind=FR_JOINT_BONDED,
        .normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
        .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=100};
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t node;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&node));
    ASSERT_EQ(node.state,FR_STRUCTURAL_OVERLOADED);
    dc_gpu_destroy(g);PASS();
}
static void test_frictionless_bearings_have_no_sideways_restraint(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);s.anchor_count=2;
    s.anchors[0].kind=FR_JOINT_BEARING;s.anchors[0].normal_angle=-1.57079632679f;
    s.anchors[0].shear_capacity=0;s.anchors[0].friction=0;
    s.anchors[1]=s.anchors[0];s.anchors[1].id=2;s.anchors[1].node=2;s.anchors[1].x=32;
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_paint_material(g,32,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t n;ASSERT_TRUE(fr_gpu_read_structure_node(g,1,&n));
    ASSERT_EQ(n.state,FR_STRUCTURAL_UNSUPPORTED);
    dc_gpu_destroy(g);PASS();
}
static void test_angled_brace_closes_force_and_moment_balance(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(20);
    ASSERT_TRUE(dc_gpu_paint_material(g,20,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_node_state_t tip;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,2,&tip));float unbraced=tip.displacement[1];
    s.node_count=4;s.member_count=2;s.joint_count=1;s.anchor_count=2;
    s.nodes[2]=s.nodes[1];s.nodes[2].id=3;s.nodes[2].y_fp=42<<16;
    s.nodes[3]=s.nodes[2];s.nodes[3].id=4;s.nodes[3].x_fp=38<<16;s.nodes[3].y_fp=50<<16;
    s.members[1]=s.members[0];s.members[1].id=2;s.members[1].a=3;s.members[1].b=4;
    s.joints[0]=(fr_structural_gpu_joint_t){.id=1,.generation=1,.a=2,.b=3,.kind=FR_JOINT_PINNED,
        .normal_stiffness=1e8,.shear_stiffness=1e8,.rotational_stiffness=1e8,
        .bearing_capacity=1e7,.pullout_capacity=1e7,.shear_capacity=1e7,.moment_capacity=1e7};
    s.anchors[1]=s.anchors[0];s.anchors[1].id=2;s.anchors[1].node=4;
    s.anchors[1].kind=FR_JOINT_PINNED;s.anchors[1].x=38;s.anchors[1].y=51;
    ASSERT_TRUE(dc_gpu_paint_material(g,38,51,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_read_structure_node(g,2,&tip));
    ASSERT_EQ(tip.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(fabs(tip.displacement[1])<unbraced*.5);
    fr_structural_gpu_node_state_t anchor;
    ASSERT_TRUE(fr_gpu_read_structure_node(g,4,&anchor));ASSERT_TRUE(fabs(anchor.reaction[0])>100);
    fr_structural_gpu_component_state_t budget;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&budget));
    for(unsigned axis=0;axis<3;axis++) ASSERT_TRUE(near(budget.external[axis]+budget.reaction[axis],0,.2));
    dc_gpu_destroy(g);PASS();
}
static void test_cold_component_diagnostics_are_unknown(void) {
    char e[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    fr_structural_gpu_graph_t s=beam(63);
    ASSERT_TRUE(dc_gpu_paint_material(g,63,41,0,DC_MATERIAL_STONE,e,sizeof(e)));
    ASSERT_TRUE(fr_gpu_place_structure(g,&s,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_set_page(g,1,0,UINT32_MAX,e,sizeof(e)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,e,sizeof(e)));
    fr_structural_gpu_component_state_t component;
    fr_structural_gpu_member_state_t member;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&component));
    ASSERT_TRUE(fr_gpu_read_structure_member(g,1,&member));
    ASSERT_EQ(component.state,FR_STRUCTURAL_UNKNOWN);
    ASSERT_EQ(member.state,FR_STRUCTURAL_UNKNOWN);
    dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_self_weight_cantilever_equilibrium);
    RUN(test_released_root_is_a_mechanism);
    RUN(test_two_pins_split_bridge_weight);
    RUN(test_centered_rigid_reactions);
    RUN(test_eccentric_rigid_reactions);
    RUN(test_material_stress_drives_overload);
    RUN(test_weak_numerical_system_is_unknown);
    RUN(test_oriented_bearing_opens_in_tension);
    RUN(test_joint_capacity_is_finite);
    RUN(test_frictionless_bearings_have_no_sideways_restraint);
    RUN(test_angled_brace_closes_force_and_moment_balance);
    RUN(test_cold_component_diagnostics_are_unknown);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
