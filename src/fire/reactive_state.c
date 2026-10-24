#include "ferrum/fire/reactive_state.h"

bool fr_reactive_state_default(uint32_t material,uint32_t mass,uint32_t water,
                                float temperature,uint64_t id,fr_reactive_state_t *out,
                                char *err,uint32_t cap) {
    (void)material;(void)mass;(void)water;(void)temperature;(void)id;(void)out;(void)err;(void)cap;return false;
}
bool fr_reactive_state_from_particle(const dc_mpm_particle_t *p,float t,uint64_t id,
                                      fr_reactive_state_t *out,char *err,uint32_t cap) {
    (void)p;(void)t;(void)id;(void)out;(void)err;(void)cap;return false;
}
bool fr_reactive_state_valid(const fr_reactive_state_t *s,char *err,uint32_t cap) {
    (void)s;(void)err;(void)cap;return false;
}
uint64_t fr_reactive_state_mass_fp(const fr_reactive_state_t *s) {(void)s;return 0;}
bool fr_reactive_state_temperature(const fr_reactive_state_t *s,float *out,char *err,uint32_t cap) {
    (void)s;(void)out;(void)err;(void)cap;return false;
}
bool fr_reactive_state_split(const fr_reactive_state_t *s,uint32_t weight,uint64_t a,uint64_t b,
                              fr_reactive_state_t *left,fr_reactive_state_t *right,char *err,uint32_t cap) {
    (void)s;(void)weight;(void)a;(void)b;(void)left;(void)right;(void)err;(void)cap;return false;
}
fr_reactive_owner_t fr_reactive_cell_owner(dc_chunk_coord_t c,uint32_t cell) {
    (void)c;(void)cell;return (fr_reactive_owner_t){0};
}
bool fr_reactive_owner_valid(const fr_reactive_owner_t *o,char *err,uint32_t cap) {
    (void)o;(void)err;(void)cap;return false;
}
