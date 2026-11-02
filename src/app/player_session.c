#include "player_session.h"

bool dc_players_enable(dc_level_view_t *v,char *err,uint32_t cap) {(void)v;(void)err;(void)cap;return false;}
bool dc_players_event(dc_level_view_t *v,const SDL_Event *e) {(void)v;(void)e;return false;}
bool dc_players_gameplay(const dc_level_view_t *v) {(void)v;return false;}
void dc_players_ui(dc_level_view_t *v,bool active) {(void)v;(void)active;}
bool dc_players_step(dc_level_view_t *v,uint64_t ms,char *err,uint32_t cap) {(void)v;(void)ms;(void)err;(void)cap;return false;}
bool dc_players_status(dc_level_view_t *v,uint32_t n,fr_player_t *p,fr_beaver_body_t *b) {(void)v;(void)n;(void)p;(void)b;return false;}
void dc_players_destroy(dc_level_view_t *v) {(void)v;}
