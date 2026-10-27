#include <stdio.h>
#include "../../src/app/bonfire.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_real_structural_pits_and_independent_rigid_stack(void) {
    dc_bonfire_plan_t p;ASSERT_TRUE(dc_bonfire_plan(&p));
    ASSERT_EQ(p.pit.member_count,10);ASSERT_EQ(p.pit.anchor_count,12);ASSERT_EQ(p.count,16);
    uint32_t logs[2]={0},sticks[2]={0},starters[2]={0};
    for(uint32_t i=0;i<p.pit.member_count;i++) {
        ASSERT_EQ(p.pit.members[i].material,FR_STRUCTURAL_STONE);ASSERT_EQ(p.pit.members[i].flags,0);
    }
    for(uint32_t i=0;i<p.count;i++) {
        const dc_bonfire_piece_t *b=p.pieces+i;uint32_t side=b->wet?1:0;
        ASSERT_TRUE(b->width>0&&b->height>0&&b->x>=142+side*192&&b->x+b->width<=178+side*192&&b->y+b->height<93);
        if(b->kind==DC_BONFIRE_LOG) {logs[side]++;ASSERT_TRUE(b->temperature_K<294&&b->y<88);}
        else if(b->kind==DC_BONFIRE_STARTER) {starters[side]++;ASSERT_TRUE(b->temperature_K>1000&&b->width*b->height<=4);}
        else {sticks[side]++;ASSERT_TRUE(b->temperature_K<294&&b->height==1&&b->y>=89);}
        for(uint32_t j=0;j<i;j++) {
            const dc_bonfire_piece_t *a=p.pieces+j;ASSERT_TRUE(a->object!=b->object);
            ASSERT_TRUE(a->x+a->width<b->x||b->x+b->width<a->x||a->y+a->height<b->y||b->y+b->height<a->y);
        }
    }
    for(uint32_t i=0;i<2;i++) {ASSERT_EQ(logs[i],3);ASSERT_EQ(sticks[i],4);ASSERT_EQ(starters[i],1);}
    for(uint32_t side=0;side<2;side++) {
        const dc_bonfire_piece_t *upper=NULL,*right=NULL;
        for(uint32_t i=0;i<p.count;i++) if(p.pieces[i].kind==DC_BONFIRE_LOG&&p.pieces[i].wet==(side!=0)) {
            if(p.pieces[i].width==16u) upper=p.pieces+i;
            else if(!right||p.pieces[i].x>right->x) right=p.pieces+i;
        }
        ASSERT_TRUE(upper&&right&&upper->x+upper->width+2u<=right->x);
    }
    PASS();
}
int main(void) {RUN(test_real_structural_pits_and_independent_rigid_stack);printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
