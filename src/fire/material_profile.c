#include <stddef.h>

#include "ferrum/fire/material_profile.h"

uint32_t fr_reactive_profile_count(void) { return 0; }
const fr_reactive_profile_t *fr_reactive_profile(uint32_t material) {
    (void)material; return NULL;
}
const fr_reactive_profile_t *fr_reactive_profile_for_body(uint32_t material) {
    (void)material; return NULL;
}
const char *fr_reactive_material_name(uint32_t material) {
    (void)material; return NULL;
}
bool fr_reactive_profile_valid(const fr_reactive_profile_t *profile,
                               char *err, uint32_t cap) {
    (void)profile; (void)err; (void)cap; return false;
}
bool fr_reactive_ignition_rates(const fr_reactive_profile_t *substrate,
                                const fr_reactive_profile_t *coating,
                                const fr_reactive_exposure_t *exposure,
                                fr_reactive_ignition_rates_t *out,
                                char *err, uint32_t cap) {
    (void)substrate; (void)coating; (void)exposure; (void)out;
    (void)err; (void)cap; return false;
}
