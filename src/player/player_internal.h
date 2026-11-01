#ifndef FERRUM_PLAYER_INTERNAL_H
#define FERRUM_PLAYER_INTERNAL_H

#include "ferrum/player/player.h"

static inline bool player_valid(const fr_player_t *p) {
    return p && p->identity && p->registry && p->registry->sealed &&
        p->registry->count<=p->registry->capacity && p->capacity>=p->registry->count &&
        (!p->registry->count || (p->registry->definitions && p->values && p->abilities));
}

static inline const fr_player_definition_t *player_definition(const fr_player_t *p,uint32_t id) {
    return player_valid(p) && id && id<=p->registry->count?&p->registry->definitions[id-1]:NULL;
}

#endif
