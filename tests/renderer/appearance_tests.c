#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/renderer/appearance.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n",#fn);fn(); } while(0)
#define ASSERT_TRUE(e) do { if(!(e)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_mixed_material_holes_and_visual_scale(void) {
    uint32_t materials[4]={DC_MATERIAL_WOOD,0,DC_MATERIAL_STONE,DC_MATERIAL_DENSE_WOOD};
    uint32_t p=99,m=99;
    ASSERT_TRUE(fr_appearance_resolve(2,2,4,3,2,materials,&p,&m));
    ASSERT_INT_EQ(p,0);ASSERT_INT_EQ(m,DC_MATERIAL_WOOD);
    ASSERT_TRUE(!fr_appearance_resolve(2,2,4,5,2,materials,&p,&m));
    ASSERT_TRUE(fr_appearance_resolve(2,2,4,7,7,materials,&p,&m));
    ASSERT_INT_EQ(p,3);ASSERT_INT_EQ(m,DC_MATERIAL_DENSE_WOOD);
    ASSERT_TRUE(!fr_appearance_resolve(2,2,4,8,0,materials,&p,&m));
    ASSERT_TRUE(!fr_appearance_resolve(2,2,0,0,0,materials,&p,&m));PASS();
}
static void test_slot_generations(void) {
    fr_appearance_owner_t owner={.identity=42,.generation=7};
    ASSERT_TRUE(fr_appearance_matches(&owner,42,7));
    ASSERT_TRUE(!fr_appearance_matches(&owner,42,8));
    ASSERT_TRUE(!fr_appearance_matches(&owner,43,7));PASS();
}
static void test_material_namespaces(void) {
    uint32_t material=99;
    ASSERT_TRUE(fr_appearance_material(FR_APPEARANCE_BODY,2,&material));
    ASSERT_INT_EQ(material,DC_MATERIAL_WOOD);
    ASSERT_TRUE(fr_appearance_material(FR_APPEARANCE_TERRAIN,2,&material));
    ASSERT_INT_EQ(material,DC_MATERIAL_SAND);
    ASSERT_TRUE(!fr_appearance_material(FR_APPEARANCE_BODY,15,&material));PASS();
}
static void test_default_and_explicit_color_independence(void) {
    fr_appearance_owner_t o={.identity=13,.generation=2,.x=-65,.y=4,.material=7,.visual_scale=1};
    fr_appearance_texel_t a=fr_appearance_default(o),b=fr_appearance_default(o);
    ASSERT_TRUE(memcmp(&a,&b,sizeof(a))==0);
    ASSERT_TRUE(a.color[0]>0&&a.normal[2]==1&&a.roughness>0);
    b.color[0]=.9f;b.color[1]=.01f;
    ASSERT_TRUE(memcmp(&a,&b,sizeof(a))!=0);ASSERT_EQ(o.material,7u);PASS();
}
static void test_rest_transform(void) {
    float x,y;fr_appearance_rest_point(10,23,10,20,1.57079632679f,&x,&y);
    ASSERT_TRUE(fabsf(x-3)<.00001f&&fabsf(y)<.00001f);PASS();
}
int main(void) {
    RUN(test_mixed_material_holes_and_visual_scale);RUN(test_slot_generations);
    RUN(test_material_namespaces);RUN(test_default_and_explicit_color_independence);RUN(test_rest_transform);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
