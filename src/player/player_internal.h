#ifndef FERRUM_PLAYER_INTERNAL_H
#define FERRUM_PLAYER_INTERNAL_H

#include <stddef.h>

#include "ferrum/player/player.h"

static inline bool player_registry_ready(const fr_player_registry_t *r) {
    return r && r->sealed && r->count<=r->capacity && (!r->count || r->definitions);
}

static inline bool player_valid(const fr_player_t *p) {
    return p && p->identity && player_registry_ready(p->registry) &&
        p->capacity>=p->registry->count && (!p->registry->count || (p->values && p->abilities));
}

static inline const fr_player_definition_t *player_definition(const fr_player_t *p,uint32_t id) {
    return player_valid(p) && id && id<=p->registry->count?&p->registry->definitions[id-1]:NULL;
}

#endif
