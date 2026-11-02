#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dungeoncraft/chunk.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/player/chew.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)) {printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static fr_chew_target_t wood(uint32_t material,double occupied,uint64_t id) {
    const fr_reactive_profile_t *p=fr_reactive_profile(material);
    return (fr_chew_target_t){id,1,material,(uint32_t)llround(p->density*occupied*65536.),true};
}
static void test_density_and_occupied_amount_control_required_work(void) {
    fr_chew_target_t targets[]={wood(DC_MATERIAL_WOOD,1,1),wood(DC_MATERIAL_DENSE_WOOD,1,2),wood(DC_MATERIAL_WOOD,.5,3)};
    uint32_t effort=targets[0].dry_mass_fp;bool ready=false;
    for(unsigned i=0;i<3;i++) {
        fr_chew_work_t work={0};
        uint64_t duration=((uint64_t)targets[i].dry_mass_fp*1000+effort-1)/effort;
        ASSERT_TRUE(fr_chew_work_advance(&work,&targets[i],effort,duration-1,true,&ready));
        ASSERT_TRUE(!ready);
        ASSERT_TRUE(fr_chew_work_advance(&work,&targets[i],effort,1,true,&ready));
        ASSERT_TRUE(ready);
    }
    ASSERT_TRUE(targets[1].dry_mass_fp>targets[0].dry_mass_fp);
    ASSERT_TRUE(targets[2].dry_mass_fp<targets[0].dry_mass_fp);
    PASS();
}
static void test_equal_elapsed_work_is_cadence_independent(void) {
    fr_chew_target_t target=wood(DC_MATERIAL_WOOD,1,1);
    fr_chew_work_t a={0},b={0};bool ready;
    ASSERT_TRUE(fr_chew_work_advance(&a,&target,13579,777,true,&ready));
    for(unsigned i=0;i<111;i++) ASSERT_TRUE(fr_chew_work_advance(&b,&target,13579,7,true,&ready));
    ASSERT_EQ(a.work,b.work);ASSERT_EQ(a.work,UINT64_C(13579)*777);
    PASS();
}
static void test_release_pauses_and_target_loss_or_generation_change_resets(void) {
    fr_chew_target_t target=wood(DC_MATERIAL_WOOD,1,1);
    fr_chew_work_t work={0};bool ready;
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,100,100,true,&ready));
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,100,UINT64_MAX,false,&ready));
    ASSERT_EQ(work.work,10000u);ASSERT_TRUE(!ready);
    target.generation++;
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,100,1,true,&ready));
    ASSERT_EQ(work.work,100u);
    target.eligible=false;
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,100,100,true,&ready));
    ASSERT_EQ(work.work,0u);ASSERT_TRUE(!ready);
    ASSERT_TRUE(fr_chew_work_advance(&work,NULL,100,100,true,&ready));
    ASSERT_EQ(work.work,0u);
    PASS();
}
static void test_stall_readiness_cannot_bank_multiple_bites_or_acknowledge_twice(void) {
    fr_chew_target_t target=wood(DC_MATERIAL_WOOD,1,1);
    fr_chew_work_t work={0};bool ready;
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,UINT32_MAX,UINT64_MAX,true,&ready));
    ASSERT_TRUE(ready);ASSERT_EQ(work.work,(uint64_t)target.dry_mass_fp*1000);
    fr_chew_target_t stale=target;stale.parcel_id++;
    ASSERT_TRUE(!fr_chew_work_committed(&work,&stale));
    ASSERT_TRUE(fr_chew_work_committed(&work,&target));
    ASSERT_TRUE(!fr_chew_work_committed(&work,&target));
    ASSERT_EQ(work.work,0u);
    ASSERT_TRUE(fr_chew_work_advance(&work,&stale,100,0,true,&ready));
    ASSERT_TRUE(!ready);ASSERT_EQ(work.work,0u);
    PASS();
}
static void test_invalid_arguments_are_transactional_and_nonwood_is_ineligible(void) {
    fr_chew_target_t target=wood(DC_MATERIAL_WOOD,1,1);fr_chew_work_t work={0};bool ready=true;
    ASSERT_TRUE(!fr_chew_work_advance(&work,&target,0,100,true,&ready));
    ASSERT_TRUE(ready);ASSERT_EQ(work.work,0u);
    ASSERT_TRUE(!fr_chew_work_advance(NULL,&target,100,100,true,&ready));
    ASSERT_TRUE(!fr_chew_work_advance(&work,&target,100,100,true,NULL));
    target.material=DC_MATERIAL_STONE;
    ASSERT_TRUE(fr_chew_work_advance(&work,&target,100,UINT64_MAX,true,&ready));
    ASSERT_TRUE(!ready);ASSERT_EQ(work.work,0u);
    PASS();
}
static void test_mass_changes_and_other_players_cannot_share_work(void) {
    fr_chew_target_t target=wood(DC_MATERIAL_WOOD,1,1);fr_chew_work_t a={0},b={0};bool ready;
    ASSERT_TRUE(fr_chew_work_advance(&a,&target,100,100,true,&ready));
    ASSERT_EQ(b.work,0u);
    target.dry_mass_fp--;
    ASSERT_TRUE(fr_chew_work_advance(&a,&target,100,1,true,&ready));
    ASSERT_EQ(a.work,100u);
    ASSERT_TRUE(!fr_chew_work_committed(&a,&target));
    ASSERT_EQ(a.work,100u);
    PASS();
}
int main(void) {
    RUN(test_density_and_occupied_amount_control_required_work);
    RUN(test_equal_elapsed_work_is_cadence_independent);
    RUN(test_release_pauses_and_target_loss_or_generation_change_resets);
    RUN(test_stall_readiness_cannot_bank_multiple_bites_or_acknowledge_twice);
    RUN(test_invalid_arguments_are_transactional_and_nonwood_is_ineligible);
    RUN(test_mass_changes_and_other_players_cannot_share_work);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
