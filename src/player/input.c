#include <string.h>
#include "ferrum/player/input.h"

void fr_input_init(fr_input_t *i) { memset(i,0,sizeof(*i)); }
bool fr_input_add_player(fr_input_t *i,uint64_t p) {(void)i;(void)p;return false;}
bool fr_input_connect(fr_input_t *i,int64_t d,uint64_t p) {(void)i;(void)d;(void)p;return false;}
bool fr_input_disconnect(fr_input_t *i,int64_t d) {(void)i;(void)d;return false;}
bool fr_input_bind(fr_input_t *i,int64_t d,uint32_t c,uint32_t a,float s) {(void)i;(void)d;(void)c;(void)a;(void)s;return false;}
bool fr_input_event(fr_input_t *i,int64_t d,uint32_t c,float v,bool r) {(void)i;(void)d;(void)c;(void)v;(void)r;return false;}
void fr_input_focus(fr_input_t *i,bool f) {(void)i;(void)f;}
bool fr_input_context(fr_input_t *i,uint64_t p,fr_input_context_t c) {(void)i;(void)p;(void)c;return false;}
bool fr_input_target(fr_input_t *i,uint64_t p,bool v,fr_player_point_t t) {(void)i;(void)p;(void)v;(void)t;return false;}
bool fr_input_sample(fr_input_t *i,uint64_t p,fr_input_state_t *o) {(void)i;(void)p;(void)o;return false;}
bool fr_player_control_tick(fr_player_control_t *c,fr_player_t *p,const fr_input_state_t *i,uint32_t f,uint32_t ice,bool w,uint64_t ms,fr_player_action_event_t *e) {(void)c;(void)p;(void)i;(void)f;(void)ice;(void)w;(void)ms;(void)e;return false;}
