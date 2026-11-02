#include <stddef.h>

#include "ferrum/player/input.h"

bool fr_player_control_tick(fr_player_control_t *c,fr_player_t *p,
    const fr_input_state_t *s,uint32_t fire,uint32_t ice,bool wood,
    uint64_t elapsed,fr_player_action_event_t *event) {
    if (!c || !p || !s || !event) return false;
    uint32_t ability=0;
    if (s->pressed&(1u<<FR_INPUT_FIRE)) ability=fire;
    else if (s->pressed&(1u<<FR_INPUT_ICE)) ability=ice;
    fr_player_intent_t intent={.move_x=s->values[FR_INPUT_MOVE],
        .jump_pressed=(s->pressed&(1u<<FR_INPUT_JUMP))!=0,
        .chew_held=s->values[FR_INPUT_CHEW]>0,
        .has_target=s->has_target,.target=s->target};
    if (p->action!=FR_PLAYER_CAST && ability && fr_player_has_ability(p,ability))
        intent.ability=ability;
    if (!fr_player_set_intent(p,intent)) return false;
    *event=(fr_player_action_event_t){0};
    if (intent.move_x!=0) p->facing=intent.move_x<0?-1:1;
    if (p->action==FR_PLAYER_CAST && fr_player_action_complete(p))
        fr_player_set_action(p,FR_PLAYER_IDLE,false);
    if (p->action!=FR_PLAYER_CAST && intent.ability) {
        c->cast=(fr_player_action_event_t){.ability=intent.ability,
            .has_target=intent.has_target,.target=intent.target};
        fr_player_set_action(p,FR_PLAYER_CAST,true);
    }
    if (p->action==FR_PLAYER_CAST) {
        if (!fr_player_advance(p,elapsed)) return false;
        if (fr_player_take_cast_event(p)) *event=c->cast;
        if (fr_player_action_complete(p)) fr_player_set_action(p,FR_PLAYER_IDLE,false);
        return true;
    }
    if (p->action==FR_PLAYER_CHEW || (intent.chew_held && intent.has_target && wood))
        return fr_player_chew_tick(p,wood,elapsed);
    if (!fr_player_set_action(p,intent.move_x!=0?FR_PLAYER_WALK:FR_PLAYER_IDLE,false)) return false;
    return fr_player_advance(p,elapsed);
}
