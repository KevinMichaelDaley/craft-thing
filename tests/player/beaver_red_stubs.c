/* Compile-only RED contract; replaced by the authored asset in GREEN. */
#include "ferrum/player/beaver.h"

bool fr_beaver_asset_size(fr_player_action_t a,size_t *b,uint32_t *e) {(void)a;(void)b;(void)e;return false;}
bool fr_beaver_asset_build(fr_player_action_t a,uint8_t *b,size_t c,uint32_t *m,uint32_t n,fr_beaver_asset_t *o,char *e,uint32_t z) {(void)a;(void)b;(void)c;(void)m;(void)n;(void)o;(void)e;(void)z;return false;}
bool fr_beaver_asset_pixel(const fr_beaver_asset_t *a,uint32_t f,uint32_t x,uint32_t y,bool flip,fr_prop_pixel_t *o,uint32_t *s) {(void)a;(void)f;(void)x;(void)y;(void)flip;(void)o;(void)s;return false;}
bool fr_beaver_source(uint32_t s,fr_beaver_source_t *o) {(void)s;(void)o;return false;}
bool fr_beaver_pixel_owners(uint32_t s,uint64_t i,uint32_t b,uint32_t g,fr_appearance_owner_t *a,fr_reactive_owner_t *r) {(void)s;(void)i;(void)b;(void)g;(void)a;(void)r;return false;}
