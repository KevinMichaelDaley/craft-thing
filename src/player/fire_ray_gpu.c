#include "ferrum/player/fire_ray_gpu.h"

bool fr_fire_ray_gpu_create(dc_gpu_t *g,const fr_player_body_binding_t *b,uint32_t n,
    fr_fire_ray_config_t c,fr_fire_ray_gpu_t **o,char *err,uint32_t cap) {
    (void)g;(void)b;(void)n;(void)c;(void)o;(void)err;(void)cap;return false;
}
bool fr_fire_ray_gpu_submit(fr_fire_ray_gpu_t *r,const fr_fire_ray_input_t *i,
    uint32_t n,float dt,char *err,uint32_t cap) {
    (void)r;(void)i;(void)n;(void)dt;(void)err;(void)cap;return false;
}
bool fr_fire_ray_gpu_stats(fr_fire_ray_gpu_t *r,uint32_t i,fr_fire_ray_stats_t *o) {(void)r;(void)i;(void)o;return false;}
void fr_fire_ray_gpu_destroy(fr_fire_ray_gpu_t *r) {(void)r;}
