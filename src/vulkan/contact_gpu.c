#include "gpu_internal.h"

bool dc_gpu_set_contact_capacity(dc_gpu_t *gpu, uint32_t capacity, char *err, uint32_t cap) {
    (void)gpu; (void)capacity; (void)err; (void)cap;
    return false;
}

bool dc_gpu_read_contacts(dc_gpu_t *gpu, dc_gpu_contact_stats_t *stats,
                          dc_gpu_contact_t *contacts, uint32_t contact_cap,
                          char *err, uint32_t cap) {
    (void)gpu; (void)stats; (void)contacts; (void)contact_cap; (void)err; (void)cap;
    return false;
}
