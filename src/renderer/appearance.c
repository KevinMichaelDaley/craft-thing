#include "ferrum/renderer/appearance.h"

bool fr_appearance_resolve(uint32_t w,uint32_t h,uint32_t s,uint32_t x,uint32_t y,
                           const uint32_t *m,uint32_t *p,uint32_t *a) {
    (void)w;(void)h;(void)s;(void)x;(void)y;(void)m;(void)p;(void)a;return false;
}
bool fr_appearance_matches(const fr_appearance_owner_t *o,uint64_t i,uint64_t g) {
    (void)o;(void)i;(void)g;return false;
}
bool fr_appearance_material(uint32_t d,uint32_t m,uint32_t *t) {
    (void)d;(void)m;(void)t;return false;
}
fr_appearance_texel_t fr_appearance_default(fr_appearance_owner_t o) {
    (void)o;return (fr_appearance_texel_t){0};
}
void fr_appearance_rest_point(float x,float y,float ox,float oy,float a,float *rx,float *ry) {
    (void)x;(void)y;(void)ox;(void)oy;(void)a;*rx=0;*ry=0;
}
