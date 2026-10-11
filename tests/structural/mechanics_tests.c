#include <math.h>
#include <stdio.h>
#include "ferrum/structural/mechanics.h"
static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(x) do { if (!(x)) {printf("FAIL %d: %s\n",__LINE__,#x);g_fail++;return;} } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
#define NEAR(a,b) ASSERT_TRUE(fabs((double)(a)-(double)(b))<1e-7*(1+fabs((double)(b))))
static fr_structural_frame_t beam(unsigned count,double angle) {
    fr_structural_frame_t f={0}; f.node_count=count+1;f.element_count=count;
    for(unsigned i=0;i<=count;i++){f.nodes[i].x=10.0*i/count*cos(angle);f.nodes[i].y=10.0*i/count*sin(angle);}
    f.nodes[0].fixed=7;f.nodes[count].force[0]=-sin(angle);f.nodes[count].force[1]=cos(angle);
    for(unsigned i=0;i<count;i++)f.elements[i]=(fr_structural_element_t){i,i+1,1000,1e15,{1,1.0/12,.5,5.0/6},false};
    return f;
}
static void test_cantilever(void){fr_structural_frame_t f=beam(1,0);fr_structural_solution_t s;ASSERT_TRUE(fr_structural_reference_solve(&f,&s));NEAR(s.displacement[1][1],4);NEAR(s.reaction[0][1],-1);NEAR(s.reaction[0][2],-10);PASS();}
static void test_rotation_refinement(void){fr_structural_solution_t a,b;fr_structural_frame_t f=beam(1,.7),g=beam(8,.7);ASSERT_TRUE(fr_structural_reference_solve(&f,&a));ASSERT_TRUE(fr_structural_reference_solve(&g,&b));NEAR(a.displacement[1][0],-4*sin(.7));NEAR(a.displacement[1][1],4*cos(.7));NEAR(a.displacement[1][0],b.displacement[8][0]);NEAR(a.displacement[1][1],b.displacement[8][1]);PASS();}
static void test_pinned_and_braced(void){fr_structural_frame_t f=beam(1,0);fr_structural_solution_t s;f.nodes[0].fixed=3;ASSERT_TRUE(!fr_structural_reference_solve(&f,&s));f.node_count=3;f.nodes[2]=(fr_structural_node_t){0,-10,{0},7};f.element_count=2;f.elements[1]=f.elements[0];f.elements[1].a=2;f.elements[1].b=1;f.elements[1].truss=true;ASSERT_TRUE(fr_structural_reference_solve(&f,&s));ASSERT_TRUE(s.residual<1e-8);PASS();}
static void test_material_namespaces(void){fr_structural_material_t m;ASSERT_TRUE(fr_structural_material_map(false,2,&m));ASSERT_INT_EQ(m,FR_STRUCTURAL_WOOD);ASSERT_TRUE(!fr_structural_material_map(true,2,&m));ASSERT_TRUE(fr_structural_material_map(true,1,&m));ASSERT_INT_EQ(m,FR_STRUCTURAL_STONE);const fr_structural_profile_t *p=fr_structural_profile(m);ASSERT_TRUE(p&&p->compression>p->tension&&p->density>0);PASS();}
static void test_strength_buckling(void){fr_structural_profile_t p={.young=1000,.tension=10,.compression=100,.shear_strength=20};fr_structural_section_t s={1,1.0/12,.5,5.0/6};NEAR(fr_structural_utilization(&p,s,20,0,0,1),2);ASSERT_TRUE(fr_structural_utilization(&p,s,-20,0,0,1)<1);ASSERT_TRUE(fr_structural_utilization(&p,s,-20,0,0,100)>1);NEAR(fr_structural_utilization(&p,s,0,0,2,1),1.2);PASS();}
static void test_interface_and_geometry(void){fr_structural_section_t s;ASSERT_TRUE(fr_structural_rectangle(2,&s));NEAR(s.area,2);NEAR(s.inertia,2.0/3);ASSERT_TRUE(!fr_structural_rectangle(0,&s));NEAR(fr_structural_attachment_utilization(-20,0,0,10,5,4,8,.5),2);NEAR(fr_structural_attachment_utilization(10,0,0,10,5,4,8,.5),2);ASSERT_TRUE(isinf(fr_structural_attachment_utilization(1,0,0,10,0,4,0,.5)));NEAR(fr_structural_attachment_utilization(-10,6,0,10,0,0,0,.5),1.2);PASS();}
static void test_joint_types(void){double k[3];fr_structural_joint_t j={.kind=FR_JOINT_BEARING,.normal_stiffness=100,.shear_stiffness=30,.rotational_stiffness=20};ASSERT_TRUE(fr_structural_joint_stiffness(j,1,k));NEAR(k[0],0);NEAR(k[1],0);NEAR(k[2],0);ASSERT_TRUE(fr_structural_joint_stiffness(j,-1,k));NEAR(k[0],100);NEAR(k[2],0);j.kind=FR_JOINT_PINNED;ASSERT_TRUE(fr_structural_joint_stiffness(j,1,k));NEAR(k[0],100);NEAR(k[2],0);j.kind=FR_JOINT_BONDED;ASSERT_TRUE(fr_structural_joint_stiffness(j,1,k));NEAR(k[2],20);PASS();}
static void test_shear_and_mass(void){fr_structural_frame_t f=beam(1,0);f.elements[0].shear=100;fr_structural_solution_t s;ASSERT_TRUE(fr_structural_reference_solve(&f,&s));NEAR(s.displacement[1][1],4.12);const fr_structural_profile_t *p=fr_structural_profile(FR_STRUCTURAL_WOOD);ASSERT_TRUE(p);double full=fr_structural_mass(p,f.elements[0].section,10);NEAR(full,6);NEAR(full,8*fr_structural_mass(p,f.elements[0].section,1.25));PASS();}
static void test_triangle_truss(void){fr_structural_frame_t f={.node_count=3,.element_count=3};f.nodes[0]=(fr_structural_node_t){0,0,{0},3};f.nodes[1]=(fr_structural_node_t){10,0,{0},2};f.nodes[2]=(fr_structural_node_t){5,5,{0,-1,0},0};for(unsigned i=0;i<3;i++)f.elements[i]=(fr_structural_element_t){i,(i+1)%3,1000,100,{1,1.0/12,.5,5.0/6},true};fr_structural_solution_t s;ASSERT_TRUE(fr_structural_reference_solve(&f,&s));NEAR(s.reaction[0][1],.5);NEAR(s.reaction[1][1],.5);PASS();}
int main(void){RUN(test_cantilever);RUN(test_rotation_refinement);RUN(test_pinned_and_braced);RUN(test_material_namespaces);RUN(test_strength_buckling);RUN(test_interface_and_geometry);RUN(test_joint_types);RUN(test_shear_and_mass);RUN(test_triangle_truss);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;}
