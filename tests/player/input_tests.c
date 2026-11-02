#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ferrum/player/input.h"

static int g_pass,g_fail;
#define RUN(fn) do { printf("RUN %s\n",#fn); fn(); } while(0)
#define ASSERT_TRUE(x) do { if(!(x)) {printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);g_fail++;return;} } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++
#define BIT(a) (1u<<(a))

static void test_ownership_and_remapping(void) {
    fr_input_t i;fr_input_state_t s;
    fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,100));
    ASSERT_TRUE(fr_input_add_player(&i,200));
    ASSERT_TRUE(fr_input_connect(&i,-1,100));
    ASSERT_TRUE(!fr_input_connect(&i,-1,200));
    ASSERT_TRUE(fr_input_connect(&i,32,200));
    ASSERT_TRUE(fr_input_bind(&i,-1,4,FR_INPUT_MOVE,-1));
    ASSERT_TRUE(fr_input_bind(&i,-1,7,FR_INPUT_MOVE,1));
    ASSERT_TRUE(fr_input_event(&i,-1,4,1,false));
    ASSERT_TRUE(fr_input_sample(&i,100,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],-1);
    ASSERT_TRUE(fr_input_sample(&i,200,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],0);
    ASSERT_TRUE(fr_input_event(&i,-1,7,1,false));
    ASSERT_TRUE(fr_input_sample(&i,100,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],0);
    ASSERT_TRUE(fr_input_bind(&i,-1,7,FR_INPUT_MOVE,-.5f));
    ASSERT_TRUE(fr_input_sample(&i,100,&s));ASSERT_EQ(s.values[FR_INPUT_MOVE],-1);
    ASSERT_TRUE(!fr_input_event(&i,-1,4,NAN,false));
    ASSERT_TRUE(!fr_input_bind(&i,-1,5,32,1));
    PASS();
}
static void test_edges_repeat_and_multiple_sources(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));ASSERT_TRUE(fr_input_connect(&i,1,1));
    ASSERT_TRUE(fr_input_bind(&i,1,1,FR_INPUT_JUMP,1));
    ASSERT_TRUE(fr_input_bind(&i,1,2,FR_INPUT_JUMP,1));
    ASSERT_TRUE(fr_input_event(&i,1,1,1,false));
    ASSERT_TRUE(fr_input_event(&i,1,1,1,true));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,BIT(FR_INPUT_JUMP));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,0);
    ASSERT_TRUE(fr_input_event(&i,1,2,1,false));
    ASSERT_TRUE(fr_input_event(&i,1,1,0,false));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.released,0);
    ASSERT_TRUE(fr_input_event(&i,1,2,0,false));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.released,BIT(FR_INPUT_JUMP));
    ASSERT_TRUE(fr_input_event(&i,1,1,1,false));ASSERT_TRUE(fr_input_event(&i,1,1,0,false));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,BIT(FR_INPUT_JUMP));ASSERT_EQ(s.released,BIT(FR_INPUT_JUMP));
    PASS();
}
static void test_clearing_context_focus_disconnect(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));ASSERT_TRUE(fr_input_connect(&i,10,1));
    ASSERT_TRUE(fr_input_bind(&i,10,3,FR_INPUT_CHEW,1));
    ASSERT_TRUE(fr_input_event(&i,10,3,1,false));
    ASSERT_TRUE(fr_input_context(&i,1,FR_INPUT_UI));
    ASSERT_TRUE(fr_input_event(&i,10,3,1,false));
    ASSERT_TRUE(fr_input_context(&i,1,FR_INPUT_GAMEPLAY));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,0);ASSERT_EQ(s.values[FR_INPUT_CHEW],0);
    ASSERT_TRUE(fr_input_event(&i,10,3,1,false));fr_input_focus(&i,false);
    ASSERT_TRUE(fr_input_event(&i,10,3,1,false));fr_input_focus(&i,true);
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.pressed,0);ASSERT_EQ(s.values[FR_INPUT_CHEW],0);
    ASSERT_TRUE(fr_input_event(&i,10,3,1,false));ASSERT_TRUE(fr_input_disconnect(&i,10));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_EQ(s.values[FR_INPUT_CHEW],0);
    ASSERT_TRUE(!fr_input_event(&i,10,3,1,false));PASS();
}
static void test_target_is_per_player(void) {
    fr_input_t i;fr_input_state_t s;fr_input_init(&i);
    ASSERT_TRUE(fr_input_add_player(&i,1));ASSERT_TRUE(fr_input_add_player(&i,2));
    fr_player_point_t t={.chunk={-900,1000},.x=3,.y=9};
    ASSERT_TRUE(fr_input_target(&i,1,true,t));
    ASSERT_TRUE(fr_input_sample(&i,1,&s));ASSERT_TRUE(s.has_target);ASSERT_EQ(s.target.chunk.x,-900);
    ASSERT_TRUE(fr_input_sample(&i,2,&s));ASSERT_TRUE(!s.has_target);
    t.x=64;ASSERT_TRUE(!fr_input_target(&i,1,true,t));PASS();
}
static void test_controls_drive_phases_and_latched_cast(void) {
    fr_player_definition_t defs[2];fr_player_registry_t r;uint32_t f,ice;
    double values[2];uint8_t abilities[2];fr_player_t p;fr_player_control_t c={0};
    ASSERT_TRUE(fr_player_registry_init(&r,defs,2));
    ASSERT_TRUE(fr_player_register(&r,"test:fire",FR_PLAYER_ABILITY,0,&f,NULL,0));
    ASSERT_TRUE(fr_player_register(&r,"test:ice",FR_PLAYER_ABILITY,0,&ice,NULL,0));
    ASSERT_TRUE(fr_player_registry_seal(&r));ASSERT_TRUE(fr_player_init(&p,1,&r,values,abilities,2));
    ASSERT_TRUE(fr_player_ability_set(&p,f,true));ASSERT_TRUE(fr_player_ability_set(&p,ice,true));
    fr_input_state_t s={.pressed=BIT(FR_INPUT_FIRE)|BIT(FR_INPUT_ICE),.has_target=true,.target={.x=10,.y=11}};
    fr_player_action_event_t event;uint32_t frame;
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,499,&event));ASSERT_EQ(event.ability,0);
    ASSERT_EQ(p.action,FR_PLAYER_CAST);ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,4);
    s.pressed=BIT(FR_INPUT_ICE);s.target.x=30;
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,201,&event));ASSERT_EQ(event.ability,f);ASSERT_EQ(event.target.x,10);
    s.pressed=0;s.values[FR_INPUT_MOVE]=-1;
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,100,&event));ASSERT_EQ(event.ability,0);
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,100,&event));ASSERT_EQ(p.action,FR_PLAYER_WALK);ASSERT_INT_EQ(p.facing,-1);
    s.values[FR_INPUT_MOVE]=0;s.values[FR_INPUT_CHEW]=1;
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,true,300,&event));ASSERT_EQ(p.action,FR_PLAYER_CHEW);
    ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,4);
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,true,300,&event));ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,4);
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,0,&event));ASSERT_TRUE(fr_player_frame(&p,&frame));ASSERT_EQ(frame,7);
    ASSERT_TRUE(fr_player_control_tick(&c,&p,&s,f,ice,false,100,&event));ASSERT_EQ(p.action,FR_PLAYER_IDLE);
    PASS();
}
int main(void) {
    RUN(test_ownership_and_remapping);RUN(test_edges_repeat_and_multiple_sources);
    RUN(test_clearing_context_focus_disconnect);RUN(test_target_is_per_player);
    RUN(test_controls_drive_phases_and_latched_cast);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
