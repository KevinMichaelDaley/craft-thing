#include <math.h>
#include <stddef.h>

#include "ferrum/player/player.h"
#include "player_internal.h"

bool fr_player_init(fr_player_t *p,uint64_t identity,const fr_player_registry_t *r,
    double *values,uint8_t *abilities,uint32_t capacity) {
    if (!p || !identity || !r || !r->sealed || r->count>r->capacity || capacity<r->count ||
        (r->count && (!r->definitions || !values || !abilities))) return false;
    for (uint32_t i=0;i<r->count;i++) {
        values[i]=r->definitions[i].initial_value;
        abilities[i]=0;
    }
    *p=(fr_player_t){.identity=identity,.registry=r,.values=values,.abilities=abilities,
        .capacity=capacity,.facing=1,.action=FR_PLAYER_IDLE};
    return true;
}

bool fr_player_value_set(fr_player_t *p,uint32_t id,double value) {
    const fr_player_definition_t *def=player_definition(p,id);
    if (!def || def->kind==FR_PLAYER_ABILITY || !isfinite(value)) return false;
    p->values[id-1]=value;
    return true;
}

bool fr_player_value_get(const fr_player_t *p,uint32_t id,double *out) {
    const fr_player_definition_t *def=player_definition(p,id);
    if (!def || def->kind==FR_PLAYER_ABILITY || !out) return false;
    *out=p->values[id-1];
    return true;
}

bool fr_player_ability_set(fr_player_t *p,uint32_t id,bool granted) {
    const fr_player_definition_t *def=player_definition(p,id);
    if (!def || def->kind!=FR_PLAYER_ABILITY) return false;
    p->abilities[id-1]=granted;
    return true;
}

bool fr_player_has_ability(const fr_player_t *p,uint32_t id) {
    const fr_player_definition_t *def=player_definition(p,id);
    return def && def->kind==FR_PLAYER_ABILITY && p->abilities[id-1]!=0;
}

bool fr_player_set_intent(fr_player_t *p,fr_player_intent_t intent) {
    if (!player_valid(p) || !isfinite(intent.move_x) || intent.move_x<-1 || intent.move_x>1 ||
        !isfinite(intent.target.x) || !isfinite(intent.target.y) ||
        intent.target.x<0 || intent.target.x>=DC_CHUNK_SIDE ||
        intent.target.y<0 || intent.target.y>=DC_CHUNK_SIDE ||
        (intent.ability && !fr_player_has_ability(p,intent.ability))) return false;
    p->intent=intent;
    return true;
}
