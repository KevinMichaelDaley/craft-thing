#include "ferrum/fire/thermal.h"
bool fr_thermal_sample_from_state(const fr_reactive_state_t *state,
    fr_thermal_sample_t *out,char *err,uint32_t cap) {
    (void)state;(void)out;(void)err;(void)cap;return false;
}
bool fr_thermal_pair_flux(fr_thermal_sample_t first,fr_thermal_sample_t second,
    float seconds,float *out,char *err,uint32_t cap) {
    (void)first;(void)second;(void)seconds;(void)out;(void)err;(void)cap;return false;
}
bool fr_thermal_probe_exchange(fr_thermal_probe_t probe,fr_thermal_sample_t target,
    float seconds,fr_thermal_probe_t *probe_out,fr_thermal_sample_t *target_out,char *err,uint32_t cap) {
    (void)probe;(void)target;(void)seconds;(void)probe_out;(void)target_out;(void)err;(void)cap;return false;
}
bool fr_thermal_absorb_pair(const fr_reactive_state_t *solid,const fr_reactive_state_t *water,
    float seconds,fr_reactive_state_t *solid_out,fr_reactive_state_t *water_out,char *err,uint32_t cap) {
    (void)solid;(void)water;(void)seconds;(void)solid_out;(void)water_out;(void)err;(void)cap;return false;
}
