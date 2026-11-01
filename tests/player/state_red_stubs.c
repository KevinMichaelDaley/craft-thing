/* Compile-only RED scaffolding; GREEN replaces this translation unit. */
#include "ferrum/player/player.h"

bool fr_player_registry_init(fr_player_registry_t *r,fr_player_definition_t *d,uint32_t c) {(void)r;(void)d;(void)c;return false;}
bool fr_player_register(fr_player_registry_t *r,const char *k,fr_player_definition_kind_t t,double v,uint32_t *id,char *e,uint32_t c) {(void)r;(void)k;(void)t;(void)v;(void)id;(void)e;(void)c;return false;}
bool fr_player_registry_seal(fr_player_registry_t *r) {(void)r;return false;}
uint32_t fr_player_find(const fr_player_registry_t *r,const char *k,fr_player_definition_kind_t t) {(void)r;(void)k;(void)t;return 0;}
bool fr_player_init(fr_player_t *p,uint64_t i,const fr_player_registry_t *r,double *v,uint8_t *a,uint32_t c) {(void)p;(void)i;(void)r;(void)v;(void)a;(void)c;return false;}
bool fr_player_value_set(fr_player_t *p,uint32_t id,double v) {(void)p;(void)id;(void)v;return false;}
bool fr_player_value_get(const fr_player_t *p,uint32_t id,double *v) {(void)p;(void)id;(void)v;return false;}
bool fr_player_ability_set(fr_player_t *p,uint32_t id,bool g) {(void)p;(void)id;(void)g;return false;}
bool fr_player_has_ability(const fr_player_t *p,uint32_t id) {(void)p;(void)id;return false;}
bool fr_player_set_intent(fr_player_t *p,fr_player_intent_t i) {(void)p;(void)i;return false;}
bool fr_player_clip(fr_player_action_t a,fr_player_clip_t *o) {(void)a;(void)o;return false;}
bool fr_player_set_action(fr_player_t *p,fr_player_action_t a,bool r) {(void)p;(void)a;(void)r;return false;}
bool fr_player_advance(fr_player_t *p,uint64_t e) {(void)p;(void)e;return false;}
bool fr_player_frame(const fr_player_t *p,uint32_t *o) {(void)p;(void)o;return false;}
bool fr_player_action_complete(const fr_player_t *p) {(void)p;return false;}
