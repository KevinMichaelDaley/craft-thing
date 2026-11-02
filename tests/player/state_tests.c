#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ferrum/player/player.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN  %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); g_fail++; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

typedef struct {
    fr_player_definition_t definitions[8];
    fr_player_registry_t registry;
    fr_player_t players[2];
    double values[2][8];
    uint8_t abilities[2][8];
    uint32_t attribute, stat, ability;
} fixture_t;

static bool fixture(fixture_t *f) {
    return fr_player_registry_init(&f->registry,f->definitions,8) &&
        fr_player_register(&f->registry,"test:attribute",FR_PLAYER_ATTRIBUTE,2,&f->attribute,NULL,0) &&
        fr_player_register(&f->registry,"test:stat",FR_PLAYER_STAT,-3,&f->stat,NULL,0) &&
        fr_player_register(&f->registry,"test:ability",FR_PLAYER_ABILITY,0,&f->ability,NULL,0) &&
        fr_player_registry_seal(&f->registry) &&
        fr_player_init(&f->players[0],41,&f->registry,f->values[0],f->abilities[0],8) &&
        fr_player_init(&f->players[1],42,&f->registry,f->values[1],f->abilities[1],8);
}

static void test_namespaced_typed_registration_and_sealing(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    ASSERT_TRUE(f.attribute && f.stat && f.ability);
    ASSERT_TRUE(f.attribute!=f.stat && f.stat!=f.ability);
    ASSERT_EQ(fr_player_find(&f.registry,"test:stat",FR_PLAYER_STAT),f.stat);
    ASSERT_EQ(fr_player_find(&f.registry,"test:stat",FR_PLAYER_ATTRIBUTE),0u);
    ASSERT_EQ(fr_player_find(&f.registry,"test:missing",FR_PLAYER_STAT),0u);
    ASSERT_TRUE(!fr_player_register(&f.registry,"more:new",FR_PLAYER_STAT,0,&f.stat,NULL,0));
    ASSERT_EQ(f.registry.count,3u);
    PASS();
}

static void test_invalid_registration_is_transactional(void) {
    fr_player_registry_t r; fr_player_definition_t defs[2];
    memset(defs,0x5a,sizeof(defs));
    ASSERT_TRUE(fr_player_registry_init(&r,defs,2));
    const char *bad[]={"","plain",":name","ns:","ns:name:extra","ns:bad key"};
    uint32_t id=99; char err[100];
    for (uint32_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        ASSERT_TRUE(!fr_player_register(&r,bad[i],FR_PLAYER_STAT,0,&id,err,sizeof(err)));
        ASSERT_TRUE(err[0]); ASSERT_EQ(id,99u); ASSERT_EQ(r.count,0u);
        ASSERT_EQ((unsigned char)defs[0].key[0],0x5au);
    }
    ASSERT_TRUE(!fr_player_register(&r,"ns:name",0,0,&id,NULL,0));
    ASSERT_TRUE(!fr_player_register(&r,"ns:name",FR_PLAYER_STAT,NAN,&id,NULL,0));
    ASSERT_TRUE(!fr_player_register(&r,"ns:name",FR_PLAYER_ABILITY,1,&id,NULL,0));
    char long_key[80]; memset(long_key,'x',sizeof(long_key)); long_key[2]=':'; long_key[79]=0;
    ASSERT_TRUE(!fr_player_register(&r,long_key,FR_PLAYER_STAT,0,&id,NULL,0));
    ASSERT_TRUE(fr_player_register(&r,"ns:name",FR_PLAYER_STAT,0,&id,NULL,0));
    ASSERT_TRUE(!fr_player_register(&r,"ns:name",FR_PLAYER_ATTRIBUTE,0,&id,NULL,0));
    ASSERT_TRUE(fr_player_register(&r,"other:name",FR_PLAYER_STAT,0,&id,NULL,0));
    ASSERT_TRUE(!fr_player_register(&r,"third:name",FR_PLAYER_STAT,0,&id,NULL,0));
    ASSERT_EQ(r.count,2u); PASS();
}

static void test_two_players_own_values_abilities_and_motion(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); double value=99;
    ASSERT_TRUE(fr_player_value_set(&f.players[0],f.attribute,8.5));
    ASSERT_TRUE(fr_player_value_get(&f.players[1],f.attribute,&value)); ASSERT_EQ(value,2);
    ASSERT_TRUE(fr_player_value_get(&f.players[0],f.attribute,&value)); ASSERT_EQ(value,8.5);
    ASSERT_TRUE(fr_player_ability_set(&f.players[0],f.ability,true));
    ASSERT_TRUE(fr_player_has_ability(&f.players[0],f.ability));
    ASSERT_TRUE(!fr_player_has_ability(&f.players[1],f.ability));
    f.players[0].position.chunk.x=INT64_MAX;
    f.players[0].velocity_x=12; f.players[0].grounded=true;
    ASSERT_EQ(f.players[1].position.chunk.x,0); ASSERT_EQ(f.players[1].velocity_x,0);
    ASSERT_TRUE(!f.players[1].grounded); ASSERT_TRUE(f.players[0].identity!=f.players[1].identity);
    ASSERT_TRUE(fr_player_ability_set(&f.players[0],f.ability,false));
    ASSERT_TRUE(!fr_player_has_ability(&f.players[0],f.ability)); PASS();
}

static void test_numeric_and_ability_lookups_reject_wrong_types(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); double out=99;
    ASSERT_TRUE(!fr_player_value_get(&f.players[0],0,&out)); ASSERT_EQ(out,99);
    ASSERT_TRUE(!fr_player_value_get(&f.players[0],f.ability,&out)); ASSERT_EQ(out,99);
    ASSERT_TRUE(!fr_player_value_set(&f.players[0],f.stat,INFINITY));
    ASSERT_TRUE(!fr_player_value_set(&f.players[0],UINT32_MAX,1));
    ASSERT_TRUE(!fr_player_value_set(&f.players[0],f.ability,1));
    ASSERT_TRUE(!fr_player_ability_set(&f.players[0],f.stat,true));
    ASSERT_TRUE(!fr_player_has_ability(&f.players[0],f.stat));
    ASSERT_TRUE(fr_player_value_get(&f.players[0],f.stat,&out)); ASSERT_EQ(out,-3); PASS();
}

static void test_init_rejects_bad_storage_without_partial_publication(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); fr_player_t before=f.players[0];
    double values[2]={7,9}; uint8_t abilities[2]={5,6};
    ASSERT_TRUE(!fr_player_init(&f.players[0],0,&f.registry,values,abilities,2));
    ASSERT_TRUE(!fr_player_init(&f.players[0],55,&f.registry,values,abilities,2));
    ASSERT_TRUE(!fr_player_init(&f.players[0],55,&f.registry,NULL,f.abilities[0],8));
    ASSERT_TRUE(memcmp(&before,&f.players[0],sizeof(before))==0);
    ASSERT_EQ(values[0],7); ASSERT_EQ(abilities[0],5);
    fr_player_registry_t unsealed=f.registry; unsealed.sealed=false;
    ASSERT_TRUE(!fr_player_init(&f.players[0],55,&unsealed,f.values[0],f.abilities[0],8)); PASS();
}

static void test_intents_validate_grants_and_canonical_world_targets(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    fr_player_intent_t intent={.move_x=-1,.jump_pressed=true,.chew_held=true,.has_target=true,
        .target={{INT64_MIN,INT64_MAX},63.5f,0}};
    ASSERT_TRUE(fr_player_set_intent(&f.players[0],intent));
    ASSERT_EQ(f.players[0].intent.target.chunk.x,INT64_MIN);
    ASSERT_EQ(f.players[1].intent.move_x,0);
    intent.ability=f.ability; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    ASSERT_TRUE(fr_player_ability_set(&f.players[0],f.ability,true));
    ASSERT_TRUE(fr_player_set_intent(&f.players[0],intent));
    fr_player_intent_t before=f.players[0].intent;
    intent.move_x=NAN; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    intent.move_x=1.01f; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    intent.move_x=0; intent.target.x=64; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    intent.target.x=-.1f; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    intent.target.x=1; intent.target.y=INFINITY; ASSERT_TRUE(!fr_player_set_intent(&f.players[0],intent));
    ASSERT_TRUE(memcmp(&before,&f.players[0].intent,sizeof(before))==0);
    ASSERT_TRUE(fr_player_set_intent(&f.players[0],(fr_player_intent_t){0}));
    ASSERT_TRUE(!f.players[0].intent.jump_pressed && !f.players[0].intent.ability); PASS();
}

static void test_exact_frame_counts_and_boundaries(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    const uint32_t counts[]={1,6,8,8}, durations[]={100,100,100,100};
    for (uint32_t action=0;action<FR_PLAYER_ACTION_COUNT;action++) {
        fr_player_clip_t clip; ASSERT_TRUE(fr_player_clip((fr_player_action_t)action,&clip));
        ASSERT_EQ(clip.frames,counts[action]); ASSERT_EQ(clip.frame_ms,durations[action]);
        if (action==FR_PLAYER_CHEW) continue;
        ASSERT_TRUE(fr_player_set_action(&f.players[0],(fr_player_action_t)action,true));
        for (uint32_t frame=0;frame<clip.frames;frame++) {
            uint32_t actual=99; ASSERT_TRUE(fr_player_frame(&f.players[0],&actual)); ASSERT_EQ(actual,frame);
            ASSERT_TRUE(fr_player_advance(&f.players[0],clip.frame_ms-1));
            ASSERT_TRUE(fr_player_frame(&f.players[0],&actual)); ASSERT_EQ(actual,frame);
            ASSERT_TRUE(fr_player_advance(&f.players[0],1));
        }
        uint32_t last; ASSERT_TRUE(fr_player_frame(&f.players[0],&last));
        ASSERT_EQ(last,clip.loop?0:clip.frames-1);
        ASSERT_EQ(fr_player_action_complete(&f.players[0]),!clip.loop);
    }
    PASS();
}

static void test_transitions_preserve_or_explicitly_restart_time(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); uint32_t frame;
    ASSERT_TRUE(fr_player_set_action(&f.players[0],FR_PLAYER_WALK,false));
    ASSERT_TRUE(fr_player_advance(&f.players[0],200));
    ASSERT_TRUE(fr_player_set_action(&f.players[0],FR_PLAYER_WALK,false));
    ASSERT_TRUE(fr_player_frame(&f.players[0],&frame)); ASSERT_EQ(frame,2u);
    ASSERT_EQ(f.players[1].action,FR_PLAYER_IDLE); ASSERT_EQ(f.players[1].elapsed_ms,0u);
    ASSERT_TRUE(fr_player_set_action(&f.players[0],FR_PLAYER_WALK,true)); ASSERT_EQ(f.players[0].elapsed_ms,0u);
    ASSERT_TRUE(fr_player_set_action(&f.players[0],FR_PLAYER_CAST,false)); ASSERT_EQ(f.players[0].elapsed_ms,0u);
    ASSERT_TRUE(!fr_player_set_action(&f.players[0],FR_PLAYER_ACTION_COUNT,true));
    ASSERT_EQ(f.players[0].action,FR_PLAYER_CAST); PASS();
}

static void test_large_intervals_and_partition_independence(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); uint32_t a,b;
    for (uint32_t i=0;i<2;i++) ASSERT_TRUE(fr_player_set_action(&f.players[i],FR_PLAYER_WALK,true));
    ASSERT_TRUE(fr_player_advance(&f.players[0],1250));
    for (uint32_t i=0;i<125;i++) ASSERT_TRUE(fr_player_advance(&f.players[1],10));
    ASSERT_TRUE(fr_player_frame(&f.players[0],&a) && fr_player_frame(&f.players[1],&b)); ASSERT_EQ(a,b);
    ASSERT_TRUE(fr_player_advance(&f.players[0],UINT64_MAX));
    ASSERT_EQ(f.players[0].elapsed_ms,(50u+UINT64_MAX%600u)%600u);
    ASSERT_TRUE(fr_player_set_action(&f.players[0],FR_PLAYER_CAST,true));
    ASSERT_TRUE(fr_player_advance(&f.players[0],UINT64_MAX));
    ASSERT_TRUE(fr_player_frame(&f.players[0],&a)); ASSERT_EQ(a,7u);
    ASSERT_TRUE(fr_player_action_complete(&f.players[0]));
    ASSERT_TRUE(fr_player_advance(&f.players[0],UINT64_MAX)); ASSERT_EQ(f.players[0].elapsed_ms,800u); PASS();
}

static void test_null_and_invalid_action_inputs(void) {
    fr_player_clip_t clip={99,99,false}; uint32_t frame=99;
    ASSERT_TRUE(!fr_player_clip((fr_player_action_t)-1,&clip)); ASSERT_EQ(clip.frames,99u);
    ASSERT_TRUE(!fr_player_clip(FR_PLAYER_WALK,NULL));
    ASSERT_TRUE(!fr_player_frame(NULL,&frame)); ASSERT_EQ(frame,99u);
    ASSERT_TRUE(!fr_player_advance(NULL,10)); ASSERT_TRUE(!fr_player_set_intent(NULL,(fr_player_intent_t){0}));
    ASSERT_TRUE(!fr_player_action_complete(NULL));
    fr_player_t empty={0}; ASSERT_TRUE(!fr_player_frame(&empty,&frame));
    ASSERT_TRUE(!fr_player_set_action(&empty,FR_PLAYER_WALK,false)); PASS();
}

static void test_chew_entry_loop_release_and_reentry(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); fr_player_t *p=&f.players[0];
    p->intent.chew_held=true; p->intent.has_target=true;
    uint32_t frame;
    const uint32_t expected[]={1,2,3,4,5,6,4,5,6,4};
    for (uint32_t i=0;i<10;i++) {
        ASSERT_TRUE(fr_player_chew_tick(p,true,i?100:0));
        ASSERT_TRUE(fr_player_frame(p,&frame)); ASSERT_EQ(frame,expected[i]);
        ASSERT_TRUE(!fr_player_action_complete(p));
    }
    p->intent.chew_held=false;
    ASSERT_TRUE(fr_player_chew_tick(p,true,0));
    ASSERT_TRUE(fr_player_frame(p,&frame)); ASSERT_EQ(frame,7u);
    ASSERT_TRUE(fr_player_chew_tick(p,true,99));
    ASSERT_TRUE(fr_player_frame(p,&frame)); ASSERT_EQ(frame,7u);
    p->intent.chew_held=true;
    ASSERT_TRUE(fr_player_chew_tick(p,true,1));
    ASSERT_EQ(p->action,FR_PLAYER_CHEW);
    ASSERT_TRUE(fr_player_chew_tick(p,true,0));
    ASSERT_TRUE(fr_player_frame(p,&frame)); ASSERT_EQ(frame,1u);
    ASSERT_EQ(f.players[1].action,FR_PLAYER_IDLE); PASS();
}

static void test_chew_invalid_target_recovers_even_during_entry(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f)); fr_player_t *p=&f.players[0];
    p->intent.chew_held=true; p->intent.has_target=true; uint32_t frame;
    ASSERT_TRUE(fr_player_chew_tick(p,false,500)); ASSERT_EQ(p->action,FR_PLAYER_IDLE);
    ASSERT_TRUE(fr_player_chew_tick(p,true,50));
    ASSERT_TRUE(fr_player_chew_tick(p,false,0));
    ASSERT_TRUE(fr_player_frame(p,&frame)); ASSERT_EQ(frame,7u);
    ASSERT_TRUE(fr_player_chew_tick(p,false,100)); ASSERT_EQ(p->action,FR_PLAYER_IDLE);
    p->intent.has_target=false;
    ASSERT_TRUE(fr_player_chew_tick(p,true,500)); ASSERT_EQ(p->action,FR_PLAYER_IDLE);
    ASSERT_TRUE(!fr_player_chew_tick(NULL,true,1)); PASS();
}

static void test_chew_partition_independence_and_overflow(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    for (uint32_t i=0;i<2;i++) {
        f.players[i].intent.chew_held=true; f.players[i].intent.has_target=true;
    }
    ASSERT_TRUE(fr_player_chew_tick(&f.players[0],true,1250));
    for (uint32_t i=0;i<125;i++) ASSERT_TRUE(fr_player_chew_tick(&f.players[1],true,10));
    uint32_t a,b;
    ASSERT_TRUE(fr_player_frame(&f.players[0],&a) && fr_player_frame(&f.players[1],&b));
    ASSERT_EQ(a,b); ASSERT_EQ(a,4u);
    ASSERT_TRUE(fr_player_chew_tick(&f.players[0],true,UINT64_MAX));
    ASSERT_TRUE(fr_player_frame(&f.players[0],&a)); ASSERT_EQ(a,4u);
    ASSERT_TRUE(fr_player_chew_tick(&f.players[0],false,UINT64_MAX));
    ASSERT_EQ(f.players[0].action,FR_PLAYER_IDLE); PASS();
}

static void test_recovery_reentry_preserves_elapsed_time_across_cadences(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    for (uint32_t i=0;i<2;i++) {
        f.players[i].intent.chew_held=true;f.players[i].intent.has_target=true;
        ASSERT_TRUE(fr_player_chew_tick(&f.players[i],true,300));
        ASSERT_TRUE(fr_player_chew_tick(&f.players[i],false,0));
    }
    ASSERT_TRUE(fr_player_chew_tick(&f.players[0],true,350));
    for (uint32_t i=0;i<35;i++) ASSERT_TRUE(fr_player_chew_tick(&f.players[1],true,10));
    uint32_t a,b;
    ASSERT_TRUE(fr_player_frame(&f.players[0],&a) && fr_player_frame(&f.players[1],&b));
    ASSERT_EQ(a,3u);ASSERT_EQ(a,b);
    ASSERT_EQ(f.players[0].elapsed_ms,f.players[1].elapsed_ms);
    PASS();
}

static void test_cast_emits_once_at_frame_six_across_skipped_samples(void) {
    fixture_t f; ASSERT_TRUE(fixture(&f));
    for (uint32_t i=0;i<2;i++) ASSERT_TRUE(fr_player_set_action(&f.players[i],FR_PLAYER_CAST,true));
    ASSERT_TRUE(fr_player_advance(&f.players[0],499));
    ASSERT_TRUE(!fr_player_take_cast_event(&f.players[0]));
    ASSERT_TRUE(fr_player_advance(&f.players[0],1));
    ASSERT_TRUE(fr_player_take_cast_event(&f.players[0]));
    ASSERT_TRUE(!fr_player_take_cast_event(&f.players[0]));
    ASSERT_TRUE(fr_player_advance(&f.players[0],UINT64_MAX));
    ASSERT_TRUE(!fr_player_take_cast_event(&f.players[0]));
    ASSERT_TRUE(fr_player_advance(&f.players[1],800));
    ASSERT_TRUE(fr_player_take_cast_event(&f.players[1]));
    ASSERT_TRUE(!fr_player_take_cast_event(&f.players[1]));
    ASSERT_TRUE(fr_player_set_action(&f.players[1],FR_PLAYER_CAST,false));
    ASSERT_TRUE(fr_player_advance(&f.players[1],800));
    ASSERT_TRUE(!fr_player_take_cast_event(&f.players[1]));
    ASSERT_TRUE(fr_player_set_action(&f.players[1],FR_PLAYER_CAST,true));
    for (uint32_t i=0;i<50;i++) ASSERT_TRUE(fr_player_advance(&f.players[1],10));
    ASSERT_TRUE(fr_player_take_cast_event(&f.players[1]));
    ASSERT_TRUE(!fr_player_take_cast_event(NULL)); PASS();
}

int main(void) {
    RUN(test_cast_emits_once_at_frame_six_across_skipped_samples);
    RUN(test_chew_entry_loop_release_and_reentry);
    RUN(test_chew_invalid_target_recovers_even_during_entry);
    RUN(test_chew_partition_independence_and_overflow);
    RUN(test_recovery_reentry_preserves_elapsed_time_across_cadences);
    RUN(test_namespaced_typed_registration_and_sealing);
    RUN(test_invalid_registration_is_transactional);
    RUN(test_two_players_own_values_abilities_and_motion);
    RUN(test_numeric_and_ability_lookups_reject_wrong_types);
    RUN(test_init_rejects_bad_storage_without_partial_publication);
    RUN(test_intents_validate_grants_and_canonical_world_targets);
    RUN(test_exact_frame_counts_and_boundaries);
    RUN(test_transitions_preserve_or_explicitly_restart_time);
    RUN(test_large_intervals_and_partition_independence);
    RUN(test_null_and_invalid_action_inputs);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail?1:0;
}
