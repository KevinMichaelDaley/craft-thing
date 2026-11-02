#include <stdio.h>

#include "../../src/app/player_clock.h"
#include "ferrum/player/input.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(x) do {if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
static void test_render_debt_cannot_multiply_into_fifteen_ticks(void) {
    dc_player_clock_plan_t p;
    ASSERT_TRUE(dc_player_clock_plan(.25,1./60,false,&p));ASSERT_EQ(p.ticks,1);
    ASSERT_EQ(p.remainder,0);
    ASSERT_TRUE(dc_player_clock_plan(0,1./60,true,&p));ASSERT_EQ(p.ticks,1);ASSERT_EQ(p.remainder,0);
    PASS();
}
static void test_normal_pacing_never_replays_a_missed_tick(void) {
    const unsigned rates[]={40,60,120};
    for(unsigned r=0;r<3;r++) {
        unsigned ticks=0;double accumulator=0;
        for(unsigned n=0;n<rates[r];n++) {
            dc_player_clock_plan_t p;accumulator+=1./rates[r];
            ASSERT_TRUE(dc_player_clock_plan(accumulator,1./60,false,&p));ticks+=p.ticks;accumulator=p.remainder;
        }
        ASSERT_EQ(ticks,rates[r]<60?rates[r]:60);
    }
    PASS();
}
static void test_repeated_stalls_cannot_replay_cast_input_or_world_ticks(void) {
    fr_player_definition_t def;fr_player_registry_t registry;uint32_t fire;
    double values[1];uint8_t abilities[1];fr_player_t p;fr_player_control_t control={0};
    ASSERT_TRUE(fr_player_registry_init(&registry,&def,1));
    ASSERT_TRUE(fr_player_register(&registry,"test:fire",FR_PLAYER_ABILITY,0,&fire,NULL,0));
    ASSERT_TRUE(fr_player_registry_seal(&registry));ASSERT_TRUE(fr_player_init(&p,1,&registry,values,abilities,1));
    ASSERT_TRUE(fr_player_ability_set(&p,fire,true));
    unsigned casts=0;double accumulator=0;
    for(unsigned frame=0;frame<80;frame++) {
        dc_player_clock_plan_t clock;accumulator+=.25;
        ASSERT_TRUE(dc_player_clock_plan(accumulator,1./60,false,&clock));
        ASSERT_EQ(clock.ticks,1);ASSERT_EQ(clock.remainder,0);accumulator=clock.remainder;
        fr_input_state_t input={.pressed=frame==0?1u<<FR_INPUT_FIRE:0};fr_player_action_event_t event;
        ASSERT_TRUE(fr_player_control_tick(&control,&p,&input,fire,0,false,16,&event));
        casts+=event.ability!=0;
    }
    ASSERT_EQ(casts,1);ASSERT_EQ(p.action,FR_PLAYER_IDLE);PASS();
}
int main(void) {RUN(test_render_debt_cannot_multiply_into_fifteen_ticks);RUN(test_normal_pacing_never_replays_a_missed_tick);RUN(test_repeated_stalls_cannot_replay_cast_input_or_world_ticks);printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
